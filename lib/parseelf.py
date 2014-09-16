# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""ELF parsing related helper functions/classes."""

from __future__ import print_function

import cStringIO
import os

from chromite.scripts import lddtree

from elftools.elf import elffile
from elftools.elf import enums
from elftools.common import utils


# Reverse dict() from numeric values to strings used to lookup st_shndx.
SH_TYPE_VALUES = dict((value, name)
                      for name, value in enums.ENUM_SH_TYPE.iteritems())


def ParseELFSymbols(elf):
  """Parses list of symbols in an ELF file.

  Args:
    elf: An elffile.ELFFile instance.

  Returns:
    A 2-tuple of (imported, exported) symbols. |imported| is a set of strings
    of undefined symbols. |exported| is a dict where the keys are defined
    symbols and the values are 3-tuples (st_info_bind, st_size, st_shndx) with
    the details of the corresponding exported symbol. Note that for imported
    symbols this information is always ('STB_GLOBAL', 0, 'SHN_UNDEF') and thus
    not included in the result.
  """
  imp = set()
  exp = dict()

  if elf.header.e_type not in ('ET_DYN', 'ET_EXEC'):
    return imp, exp

  for segment in elf.iter_segments():
    if segment.header.p_type != 'PT_DYNAMIC':
      continue

    # Find strtab and symtab virtual addresses.
    strtab_ptr = None
    symtab_ptr = None
    symbol_size = elf.structs.Elf_Sym.sizeof()
    for tag in segment.iter_tags():
      if tag.entry.d_tag == 'DT_SYMTAB':
        symtab_ptr = tag.entry.d_ptr
      if tag.entry.d_tag == 'DT_STRTAB':
        strtab_ptr = tag.entry.d_ptr
      if tag.entry.d_tag == 'DT_SYMENT':
        assert symbol_size == tag.entry.d_val

    stringtable = segment._get_stringtable()  # pylint: disable=W0212

    symtab_offset = next(elf.address_offsets(symtab_ptr))
    # Assume that symtab ends right before strtab.
    # This is the same assumption that glibc makes in dl-addr.c.
    # The first symbol is always local undefined, unnamed so we ignore it.
    for i in range(1, (strtab_ptr - symtab_ptr) / symbol_size):
      symbol_offset = symtab_offset + (i * symbol_size)
      symbol = utils.struct_parse(elf.structs.Elf_Sym, elf.stream,
                                  symbol_offset)
      if symbol['st_info']['bind'] == 'STB_LOCAL':
        # Ignore local symbols.
        continue
      symbol_name = stringtable.get_string(symbol.st_name)
      if symbol['st_shndx'] == 'SHN_UNDEF':
        if symbol['st_info']['bind'] == 'STB_GLOBAL':
          # Global undefined --> required symbols.
          # We ignore weak undefined symbols.
          imp.add(symbol_name)
      elif symbol['st_other']['visibility'] == 'STV_DEFAULT':
        # Exported symbols must have default visibility.
        st_shndx = SH_TYPE_VALUES.get(symbol['st_shndx'], symbol['st_shndx'])
        exp[symbol_name] = (symbol['st_info']['bind'], symbol['st_size'],
                            st_shndx)
  return imp, exp


def ParseELF(root, rel_path, ldpaths=None, parse_symbols=True):
  """Parse the ELF file.

  Loads and parses the passed elf file.

  Args:
    root: Path to the directory where the rootfs is mounted.
    rel_path: The path to the parsing file relative to root.
    ldpaths: The dict() with the ld path information. See lddtree.LoadLdpaths()
        for details.
    parse_symbols: Whether the result includes the dynamic symbols 'imp_sym' and
        'exp_sym' sections. Disabling it reduces the time for large files with
        many symbols.

  Returns:
    If the passed file isn't a supported ELF file, returns None. Otherwise,
    returns a dict() with information about the parsed ELF.
  """
  # Ensure root has a trailing / so removing the root prefix also removes any
  # / from the beginning of the path.
  root = root.rstrip('/') + '/'

  with open(os.path.join(root, rel_path), 'rb') as f:
    if f.read(4) != '\x7fELF':
      # Ignore non-ELF files. This check is done to speedup the process.
      return
    f.seek(0)
    # Continue reading and cache the whole file to speedup seeks.
    stream = cStringIO.StringIO(f.read())

  try:
    elf = elffile.ELFFile(stream)
  except elffile.ELFError:
    # Ignore unsupported ELF files.
    return
  if elf.header.e_type == 'ET_REL':
    # Don't parse relocatable ELF files (mostly kernel modules).
    return {
        'type': elf.header.e_type,
        'realpath': rel_path,
    }

  if ldpaths is None:
    ldpaths = lddtree.LoadLdpaths(root)

  result = lddtree.ParseELF(os.path.join(root, rel_path), root=root,
                            ldpaths=ldpaths)
  # Convert files to relative paths.
  for libdef in result['libs'].values():
    for path in ('realpath', 'path'):
      if not libdef[path] is None and libdef[path].startswith(root):
        libdef[path] = libdef[path][len(root):]

  for path in ('interp', 'realpath'):
    if not result[path] is None and result[path].startswith(root):
      result[path] = result[path][len(root):]

  result['type'] = elf.header.e_type
  result['sections'] = dict((str(sec.name), sec['sh_size'])
                            for sec in elf.iter_sections())
  result['segments'] = set(seg['p_type'] for seg in elf.iter_segments())

  # Some libraries (notably, the libc, which you can execute as a normal
  # binary) have the interp set. We use the file extension in those cases
  # because exec files shouldn't have a .so extension.
  result['is_lib'] = ((result['interp'] is None or rel_path[-3:] == '.so') and
                      elf.header.e_type == 'ET_DYN')

  if parse_symbols:
    result['imp_sym'], result['exp_sym'] = ParseELFSymbols(elf)
  return result

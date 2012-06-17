#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


#
# Default macros for various platforms.
#
NEWLIB_DEFAULTS = """
NEWLIB_CC?=$(TC_PATH)/$(OSNAME)_x86_newlib/bin/i686-nacl-gcc -c
NEWLIB_CXX?=$(TC_PATH)/$(OSNAME)_x86_newlib/bin/i686-nacl-g++ -c
NEWLIB_LINK?=$(TC_PATH)/$(OSNAME)_x86_newlib/bin/i686-nacl-g++ -Wl,-as-needed
NEWLIB_DUMP?=$(TC_PATH)/$(OSNAME)_x86_newlib/x86_64-nacl/bin/objdump
"""

GLIBC_DEFAULTS = """
GLIBC_CC?=$(TC_PATH)/$(OSNAME)_x86_glibc/bin/i686-nacl-gcc -c
GLIBC_CXX?=$(TC_PATH)/$(OSNAME)_x86_glibc/bin/i686-nacl-g++ -c
GLIBC_LINK?=$(TC_PATH)/$(OSNAME)_x86_glibc/bin/i686-nacl-g++ -Wl,-as-needed
GLIBC_DUMP?=$(TC_PATH)/$(OSNAME)_x86_glibc/x86_64-nacl/bin/objdump
GLIBC_PATHS:=-L $(TC_PATH)/$(OSNAME)_x86_glibc/x86_64-nacl/lib32
GLIBC_PATHS+=-L $(TC_PATH)/$(OSNAME)_x86_glibc/x86_64-nacl/lib
"""

PNACL_DEFAULTS = """
PNACL_CC?=$(TC_PATH)/$(OSNAME)_x86_pnacl/newlib/bin/pnacl-clang -c
PNACL_CXX?=$(TC_PATH)/$(OSNAME)_x86_pnacl/newlib/bin/pnacl-clang++ -c
PNACL_LINK?=$(TC_PATH)/$(OSNAME)_x86_pnacl/newlib/bin/pnacl-clang++
PNACL_DUMP?=$(TC_PATH)/$(OSNAME)_x86_pnacl/newlib/bin/objdump
TRANSLATE:=$(TC_PATH)/$(OSNAME)_x86_pnacl/newlib/bin/pnacl-translate
"""


#
# Compile rules for various platforms.
#
NEXE_CC_RULE = """
<OBJS>:=$(patsubst %.<ext>, <tc>/%_<ARCH>.o,$(<PROJ>_<EXT>))
$(<OBJS>) : <tc>/%_<ARCH>.o : %.<ext> $(THIS_MAKE) | <tc>
<TAB>$(<CC>) -o $@ $< <MACH> $(<PROJ>_<EXT>FLAGS) -DTCNAME=<tc>
"""

SO_CC_RULE = """
<OBJS>:=$(patsubst %.<ext>, <tc>/%_<ARCH>.o,$(<PROJ>_<EXT>))
$(<OBJS>) : <tc>/%_<ARCH>.o : %.<ext> $(THIS_MAKE) | <tc>
<TAB>$(<CC>) -o $@ $< <MACH> -fPIC $(<PROJ>_<EXT>FLAGS) -DTCNAME=<tc>
"""

#
# Link rules for various platforms.
#
NEXE_LINK_RULE = """
<tc>/<proj>_<ARCH>.<ext> : <OBJS>
<TAB>$(<LINK>) -o $@ $^ <MACH> $(<PROJ>_LDFLAGS)
<TC>_NMF+=<tc>/<proj>_<ARCH>.<ext> 
"""

PEXE_LINK_RULE = """
<tc>/<proj>.pexe : <OBJS>
<TAB>$(<LINK>) -o $@ $^ $(<PROJ>_LDFLAGS)

<tc>/<proj>_x86_32.nexe : <tc>/<proj>.pexe
<TAB>$(TRANSLATE) -arch x86-32 $< -o $@ 

<tc>/<proj>_x86_64.nexe : <tc>/<proj>.pexe
<TAB>$(TRANSLATE) -arch x86-64 $< -o $@ 

<tc>/<proj>_arm.nexe : <tc>/<proj>.pexe
<TAB>$(TRANSLATE) -arch arm $< -o $@ 
PNACL_NMF:=<tc>/<proj>_x86_32.nexe <tc>/<proj>_x86_64.nexe <tc>/<proj>_arm.nexe
"""

SO_LINK_RULE = """
<tc>/<proj>_<ARCH>.so : <OBJS>
<TAB>$(<LINK>) -o $@ $^ <MACH> -shared $(<PROJ>_LDFLAGS)
GLIBC_REMAP+= -n <proj>_<ARCH>.so,<proj>.so
<TC>_NMF+=<tc>/<proj>_<ARCH>.so
"""


NMF_RULE = """
<tc>/<proj>.nmf : $(<TC>_NMF)
<TAB>$(NMF) -D $(<DUMP>) -o $@ $^ -t <tc> -s <tc>

"""

GLIBC_NMF_RULE = """
<tc>/<proj>.nmf : $(<TC>_NMF)
<TAB>$(NMF) -D $(<DUMP>) -o $@ $(GLIBC_PATHS) $^ -t <tc> -s <tc> $(<TC>_REMAP)

"""

EXT_MAP = {
  'c': 'CC',
  'cc': 'CXX'
}

#
# Various Architectures
#
NACL_X86_32 = {
  '<arch>': '32',
  '<ARCH>': 'x86_32',
  '<MACH>': '-m32',
}
NACL_X86_64 = {
  '<arch>': '64',
  '<ARCH>': 'x86_64',
  '<MACH>': '-m64',
}
NACL_PNACL = {
  '<arch>': 'pnacl',
  '<ARCH>': 'PNACL',
  '<MACH>': '',
}


BUILD_RULES = {
  'newlib' : {
    'ARCHES': [NACL_X86_32, NACL_X86_64],
    'DEFS': NEWLIB_DEFAULTS,
    'CC' : NEXE_CC_RULE,
    'CXX' : NEXE_CC_RULE,
    'nmf' : NMF_RULE,
    'main': NEXE_LINK_RULE,
    'so' : None,
  },
  'glibc' : {
    'ARCHES': [NACL_X86_32, NACL_X86_64],
    'DEFS': GLIBC_DEFAULTS,
    'CC': NEXE_CC_RULE,
    'CXX': NEXE_CC_RULE,
    'nmf' : GLIBC_NMF_RULE,
    'main': NEXE_LINK_RULE,
    'so': SO_LINK_RULE,
  },
  'pnacl' : {
    'ARCHES': [NACL_PNACL],
    'DEFS': PNACL_DEFAULTS,
    'CC': NEXE_CC_RULE,
    'CXX': NEXE_CC_RULE,
    'nmf' : NMF_RULE,
    'main': PEXE_LINK_RULE,
    'so': None,
  },
}


def BuildToolDict(toolchain, project, arch = {}, ext='nexe', **kwargs):
  tc = toolchain
  TC = toolchain.upper()
  proj = project
  PROJ = proj.upper()
  EXT = EXT_MAP.get(ext, ext.upper())

  replace = {
    '<CC>' : '%s_%s' % (TC, EXT),
    '<DUMP>': '%s_DUMP' % TC,
    '<ext>' : ext,
    '<EXT>' : EXT,
    '<LINK>': '%s_LINK' % TC,
    '<proj>': proj,
    '<PROJ>': PROJ,
    '<TAB>': '\t',
    '<tc>' : tc,
    '<TC>' : TC
  }

    # Add replacements for this platform/architecture
  for key in arch:
    replace[key] = arch[key]

  # Add other passed in replacements
  for key in kwargs:
    replace['<%s>' % key] = kwargs[key]
    
  if '<OBJS>' not in replace:
    if replace.get('<ARCH>', ''):
      replace['<OBJS>'] = '%s_%s_%s_%s_O' % (TC, PROJ, replace['<ARCH>'], EXT)
    else:
      replace['<OBJS>'] = '%s_%s_%s_O' % (TC, PROJ, EXT)

  return replace





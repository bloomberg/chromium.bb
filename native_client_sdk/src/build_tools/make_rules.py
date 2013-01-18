#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

# pylint: disable=C0301
# This file contains lines longer than 80

#
# Default macros for various platforms.
#
NEWLIB_DEFAULTS = """
NEWLIB_CC?=$(TC_PATH)/$(OSNAME)_x86_newlib/bin/i686-nacl-gcc -c
NEWLIB_CXX?=$(TC_PATH)/$(OSNAME)_x86_newlib/bin/i686-nacl-g++ -c
NEWLIB_LINK?=$(TC_PATH)/$(OSNAME)_x86_newlib/bin/i686-nacl-g++ -Wl,-as-needed
NEWLIB_LIB?=$(TC_PATH)/$(OSNAME)_x86_newlib/bin/i686-nacl-ar r
NEWLIB_CCFLAGS?=-MMD -pthread $(NACL_WARNINGS) -idirafter $(NACL_SDK_ROOT)/include
NEWLIB_LDFLAGS?=-pthread
"""

ARM_DEFAULTS = """
ARM_CC?=$(TC_PATH)/$(OSNAME)_arm_newlib/bin/arm-nacl-gcc -c
ARM_CXX?=$(TC_PATH)/$(OSNAME)_arm_newlib/bin/arm-nacl-g++ -c
ARM_LINK?=$(TC_PATH)/$(OSNAME)_arm_newlib/bin/arm-nacl-g++ -Wl,-as-needed
ARM_LIB?=$(TC_PATH)/$(OSNAME)_arm_newlib/bin/arm-nacl-ar r
"""

GLIBC_DEFAULTS = """
GLIBC_CC?=$(TC_PATH)/$(OSNAME)_x86_glibc/bin/i686-nacl-gcc -c
GLIBC_CXX?=$(TC_PATH)/$(OSNAME)_x86_glibc/bin/i686-nacl-g++ -c
GLIBC_LINK?=$(TC_PATH)/$(OSNAME)_x86_glibc/bin/i686-nacl-g++ -Wl,-as-needed
GLIBC_LIB?=$(TC_PATH)/$(OSNAME)_x86_glibc/bin/i686-nacl-ar r
GLIBC_DUMP?=$(TC_PATH)/$(OSNAME)_x86_glibc/x86_64-nacl/bin/objdump
GLIBC_PATHS:=-L $(TC_PATH)/$(OSNAME)_x86_glibc/x86_64-nacl/lib32
GLIBC_PATHS+=-L $(TC_PATH)/$(OSNAME)_x86_glibc/x86_64-nacl/lib
GLIBC_CCFLAGS?=-MMD -pthread $(NACL_WARNINGS) -idirafter $(NACL_SDK_ROOT)/include
GLIBC_LDFLAGS?=-pthread
"""

PNACL_DEFAULTS = """
PNACL_CC?=$(TC_PATH)/$(OSNAME)_x86_pnacl/newlib/bin/pnacl-clang -c
PNACL_CXX?=$(TC_PATH)/$(OSNAME)_x86_pnacl/newlib/bin/pnacl-clang++ -c
PNACL_LINK?=$(TC_PATH)/$(OSNAME)_x86_pnacl/newlib/bin/pnacl-clang++
PNACL_LIB?=$(TC_PATH)/$(OSNAME)_x86_pnacl/newlib/bin/pnacl-ar r
PNACL_CCFLAGS?=-MMD -pthread $(NACL_WARNINGS) -idirafter $(NACL_SDK_ROOT)/include
PNACL_LDFLAGS?=-pthread
TRANSLATE:=$(TC_PATH)/$(OSNAME)_x86_pnacl/newlib/bin/pnacl-translate
"""

LINUX_DEFAULTS = """
LINUX_WARNINGS?=-Wno-long-long
LINUX_CC?=gcc -c
LINUX_CXX?=g++ -c
LINUX_LINK?=g++
LINUX_LIB?=ar r
LINUX_CCFLAGS=-MMD -pthread $(LINUX_WARNINGS) -I$(NACL_SDK_ROOT)/include -I$(NACL_SDK_ROOT)/include/linux
"""

WIN_DEFAULTS = """
WIN_CC?=cl.exe /nologo /WX
WIN_CXX?=cl.exe /nologo /EHsc /WX
WIN_LINK?=link.exe /nologo
WIN_LIB?=lib.exe /nologo
WIN_CCFLAGS=/I$(NACL_SDK_ROOT)\\include /I$(NACL_SDK_ROOT)\\include\\win -D WIN32 -D _WIN32 -D PTW32_STATIC_LIB
"""

#
# Compile rules for various platforms.
#
NACL_CC_RULES = {
  'Debug': '<TAB>$(<CC>) -o $@ $< -g -O0 <MACH> $(<TC>_CCFLAGS) $(<PROJ>_<EXT>FLAGS) <DEFLIST> <INCLIST>',
  'Release': '<TAB>$(<CC>) -o $@ $< -O2 <MACH> $(<TC>_CCFLAGS) $(<PROJ>_<EXT>FLAGS) <DEFLIST> <INCLIST>',
}

SO_CC_RULES = {
  'Debug': '<TAB>$(<CC>) -o $@ $< -g -O0 <MACH> -fPIC $(<TC>_CCFLAGS) $(<PROJ>_<EXT>FLAGS) <DEFLIST> <INCLIST>',
  'Release': '<TAB>$(<CC>) -o $@ $< -O2 <MACH> -fPIC $(<TC>_CCFLAGS) $(<PROJ>_<EXT>FLAGS) <DEFLIST> <INCLIST>'
}

WIN_CC_RULES = {
  'Debug': '<TAB>$(<CC>) /Od /Fo$@ /MTd /Z7 /c $< $(WIN_CCFLAGS) <DEFLIST> <INCLIST>',
  'Release': '<TAB>$(<CC>) /O2 /Fo$@ /MT /c $< $(WIN_CCFLAGS) <DEFLIST> <INCLIST>'
}

#
# Link rules for various platforms.
#
NEXE_LINK_RULES = {
  'Debug': '<TAB>$(<LINK>) -o $@ $^ -g <MACH> $(<TC>_LDFLAGS) $(<PROJ>_LDFLAGS) -L$(NACL_SDK_ROOT)/lib/<libdir>/<config> -Wl,--start-group <LIBLIST> -Wl,--end-group',
  'Release': '<TAB>$(<LINK>) -o $@ $^ <MACH> $(<TC>_LDFLAGS) $(<PROJ>_LDFLAGS) -L$(NACL_SDK_ROOT)/lib/<libdir>/<config>  -Wl,--start-group <LIBLIST> -Wl,--end-group'
}

SO_LINK_RULES = {
  'Debug': '<TAB>$(<LINK>) -o $@ $^ -g <MACH> -shared $(<PROJ>_LDFLAGS) -L$(NACL_SDK_ROOT)/lib/<libdir>/<config>',
  'Release': '<TAB>$(<LINK>) -o $@ $^ <MACH> -shared $(<PROJ>_LDFLAGS) -L$(NACL_SDK_ROOT)/lib/<libdir>/<config>',
}

LINUX_SO_LINK_RULES = {
  'Debug': '<TAB>$(<LINK>) -o $@ $^ -g <MACH> -shared $(<PROJ>_LDFLAGS) -L$(NACL_SDK_ROOT)/lib/$(OSNAME)_<tcname>/<config> -Wl,--whole-archive <LIBLIST> -Wl,--no-whole-archive',
  'Release': '<TAB>$(<LINK>) -o $@ $^ <MACH> -shared $(<PROJ>_LDFLAGS) -L$(NACL_SDK_ROOT)/lib/$(OSNAME)_<tcname>/<config> -Wl,--whole-archive <LIBLIST> -Wl,--no-whole-archive',
}

PEXE_TRANSLATE_RULE = """
<tc>/<config>/<proj>_x86_32.nexe : <tc>/<config>/<proj>.pexe
<TAB>$(TRANSLATE) -arch x86-32 $< -o $@

<tc>/<config>/<proj>_x86_64.nexe : <tc>/<config>/<proj>.pexe
<TAB>$(TRANSLATE) -arch x86-64 $< -o $@

<tc>/<config>/<proj>_arm.nexe : <tc>/<config>/<proj>.pexe
<TAB>$(TRANSLATE) -arch arm $< -o $@"""

PEXE_LINK_RULES = {
  'Debug': '<TAB>$(<LINK>) -o $@ $^ -g $(<TC>_LDFLAGS) $(<PROJ>_LDFLAGS) -L$(NACL_SDK_ROOT)/lib/<libdir>/<config> <LIBLIST>\n' + PEXE_TRANSLATE_RULE,
  'Release': '<TAB>$(<LINK>) -o $@ $^ $(<TC>_LDFLAGS) $(<PROJ>_LDFLAGS) -L$(NACL_SDK_ROOT)/lib/<libdir>/<config> <LIBLIST>\n' + PEXE_TRANSLATE_RULE,
}

WIN_LINK_RULES = {
  'Debug': '<TAB>$(<LINK>) /DLL /OUT:$@ /PDB:$@.pdb $(<PROJ>_LDFLAGS) /DEBUG /LIBPATH:$(NACL_SDK_ROOT)/lib/win_host/Debug $^ <LIBLIST> $(WIN_LDFLAGS)',
  'Release': '<TAB>$(<LINK>) /DLL /OUT:$@ $(<PROJ>_LDFLAGS) /LIBPATH:$(NACL_SDK_ROOT)/lib/win_host/Release $^ <LIBLIST> $(WIN_LDFLAGS)'
}


#
# Lib rules for various platforms.
#
POSIX_LIB_RULES = {
  'Debug':
      '<TAB>$(MKDIR) -p $(dir $@)\n'
      '<TAB>$(<LIB>) $@ $^',
  'Release':
      '<TAB>$(MKDIR) -p $(dir $@)\n'
      '<TAB>$(<LIB>) $@ $^',
}

WIN_LIB_RULES = {
  'Debug':
      '<TAB>$(MKDIR) -p $(dir $@)\n'
      '<TAB>$(<LIB>) /OUT:$@ $^ $(WIN_LDFLAGS) <LIBLIST>',
  'Release':
      '<TAB>$(MKDIR) -p $(dir $@)\n'
	  '<TAB>$(<LIB>) /OUT:$@ $^ $(WIN_LDFLAGS) <LIBLIST>'
}


#
# NMF rules for various platforms.
#
NMF_RULE = """
<tc>/<config>/<proj>.nmf : <NMF_TARGETS>
<TAB>$(NMF) -o $@ $^ -s <tc>/<config>
"""

NMF_EMPTY = """
<tc>/<config>/<proj>.nmf : <NMF_TARGETS> | <tc>/<config>
<TAB>echo {} > $@
"""

GLIBC_NMF_RULE = """
<tc>/<config>/<proj>.nmf : <NMF_TARGETS>
<TAB>$(NMF) -D $(<OBJDUMP>) -o $@ $(GLIBC_PATHS) $^ -s <tc>/<config> $(GLIBC_REMAP)
"""

EXT_MAP = {
  'c': 'CC',
  'cc': 'CXX'
}

WIN_TOOL = {
  'DEFINE': '-D%s',
  'INCLUDE': '/I%s',
  'LIBRARY': '%s.lib',
  'MAIN': '<tc>/<config>/<proj>.dll',
  'NMFMAIN': '<tc>/<config>/<proj>.dll',
  'SO': None,
  'LIB': '$(NACL_SDK_ROOT)/lib/win_host/<config>/<proj>.lib',
}

LINUX_TOOL = {
  'DEFINE': '-D%s',
  'INCLUDE': '-I%s',
  'LIBRARY': '-l%s',
  'MAIN': '<tc>/<config>/lib<proj>.so',
  'NMFMAIN': '<tc>/<config>/lib<proj>.so',
  'SO': '<tc>/<config>/lib<proj>.so',
  'LIB': '$(NACL_SDK_ROOT)/lib/linux_host/<config>/lib<proj>.a',
}

NACL_TOOL = {
  'DEFINE': '-D%s',
  'INCLUDE': '-I%s',
  'LIBRARY': '-l%s',
  'MAIN': '<tc>/<config>/<proj>_<ARCH>.nexe',
  'NMFMAIN': '<tc>/<config>/<proj>_<ARCH>.nexe',
  'SO': '<tc>/<config>/<proj>_<ARCH>.so',
  'LIB': '$(NACL_SDK_ROOT)/lib/<libdir>/<config>/lib<proj>.a',
}

PNACL_TOOL = {
  'DEFINE': '-D%s',
  'INCLUDE': '-I%s',
  'LIBRARY': '-l%s',
  'MAIN': '<tc>/<config>/<proj>.pexe',
  'NMFMAIN':
      '<tc>/<config>/<proj>_x86_32.nexe '
      '<tc>/<config>/<proj>_x86_64.nexe '
      '<tc>/<config>/<proj>_arm.nexe',
  'SO': None,
  'LIB': '$(NACL_SDK_ROOT)/lib/<libdir>/<config>/lib<proj>.a',
}


#
# Various Architectures
#
LINUX = {
  '<arch>': 'linux',
  '<ARCH>': '',
  '<MACH>': '',
}
NACL_X86_32 = {
  '<arch>': 'x86',
  '<ARCH>': 'x86_32',
  '<MACH>': '-m32',
}
NACL_X86_64 = {
  '<arch>': 'x64',
  '<ARCH>': 'x86_64',
  '<MACH>': '-m64',
}
NACL_PNACL = {
  '<arch>': 'pnacl',
  '<MACH>': '',
}
NACL_ARM = {
  '<arch>': 'arm',
  '<ARCH>': 'arm',
  '<MACH>': '',
  # override the default NACL toolchain
  '<TOOLSET>': 'ARM',
}
WIN_32 = {
  '<arch>': '',
  '<ARCH>': 'x86_32',
  '<MACH>': '',
}


BUILD_RULES = {
  'newlib' : {
    'ARCHES': [NACL_X86_32, NACL_X86_64, NACL_ARM],
    'DEFS': NEWLIB_DEFAULTS + ARM_DEFAULTS,
    'CC' : NACL_CC_RULES,
    'CXX' : NACL_CC_RULES,
    'NMF' : NMF_RULE,
    'MAIN': NEXE_LINK_RULES,
    'LIB': POSIX_LIB_RULES,
    'SO' : None,
    'TOOL': NACL_TOOL,
  },
  'glibc' : {
    'ARCHES': [NACL_X86_32, NACL_X86_64],
    'DEFS': GLIBC_DEFAULTS,
    'CC': NACL_CC_RULES,
    'CXX': NACL_CC_RULES,
    'NMF' : GLIBC_NMF_RULE,
    'MAIN': NEXE_LINK_RULES,
    'LIB': POSIX_LIB_RULES,
    'SO': SO_LINK_RULES,
    'TOOL': NACL_TOOL,
  },
  'pnacl' : {
    'ARCHES': [NACL_PNACL],
    'DEFS': PNACL_DEFAULTS,
    'CC': NACL_CC_RULES,
    'CXX': NACL_CC_RULES,
    'NMF' : NMF_RULE,
    'MAIN': PEXE_LINK_RULES,
    'LIB': POSIX_LIB_RULES,
    'SO': None,
    'TOOL': PNACL_TOOL
  },
  'win' : {
    'ARCHES': [WIN_32],
    'DEFS': WIN_DEFAULTS,
    'CC': WIN_CC_RULES,
    'CXX': WIN_CC_RULES,
    'NMF' : NMF_EMPTY,
    'MAIN': WIN_LINK_RULES,
    'LIB': WIN_LIB_RULES,
    'SO': None,
    'TOOL': WIN_TOOL
  },
  'linux' : {
    'ARCHES': [LINUX],
    'DEFS': LINUX_DEFAULTS,
    'CC': SO_CC_RULES,
    'CXX': SO_CC_RULES,
    'NMF' : NMF_EMPTY,
    'MAIN': LINUX_SO_LINK_RULES,
    'LIB': POSIX_LIB_RULES,
    'SO': None,
    'TOOL': LINUX_TOOL
  }
}

class MakeRules(object):
  """MakeRules generates Tool, Config, and Arch dependend makefile settings.

  The MakeRules object generates strings used in the makefile based on the
  current object settings such as toolchain, configuration, architecture...
  It stores settings such as includes, defines, and lists, and converts them
  to the appropriate format whenever the toolchain changes.
  """

  def __init__(self, tc):
    self.tc = tc
    self.project = ''
    self.cfg = ''
    self.ptype = ''
    self.arch_ext = ''
    self.defines = []
    self.includes = []
    self.libraries = []
    self.vars = {
      '<TAB>': '\t',
    }
    self.SetToolchain(tc)

  def _BuildList(self, key, items):
    pattern = BUILD_RULES[self.tc]['TOOL'][key]
    if pattern and items:
      items = [pattern % item for item in items]
      return ' '.join(items)
    return ''

  def BuildDefaults(self):
    return BUILD_RULES[self.tc]['DEFS']

  def BuildDirectoryRules(self, configs):
    tc = self.tc
    rules = '\n#\n# Rules for %s toolchain\n#\n%s:\n\t$(MKDIR) %s\n' % (
        tc, tc, tc)
    for cfg in configs:
      rules += '%s/%s: | %s\n\t$(MKDIR) %s/%s\n' % (tc, cfg, tc, tc, cfg)

    rules += '\n# Include header dependency files.\n'
    for cfg in configs:
      rules += '-include %s/%s/*.d\n' % (tc, cfg)
    return rules + '\n'

  def BuildCompileRule(self, ext, src):
    self.vars['<EXT>'] = ext
    objname = '%s%s.o' % (os.path.splitext(src)[0], self.arch_ext)
    out = '<tc>/<config>/%s : %s $(THIS_MAKE) | <tc>/<config>\n' % (objname,
        src)
    rule = BUILD_RULES[self.tc][ext][self.cfg]
    if ext == 'CXX':
      rule = rule.replace('<CC>', '<CXX>')
    out += rule + '\n\n'
    return self.Replace(out)

  def BuildLinkRule(self):
    target = BUILD_RULES[self.tc]['TOOL'][self.ptype.upper()]
    out = ''
    if self.ptype == 'lib':
      out = 'ALL_TARGETS+=%s\n' % target
    out += target + (' : $(<PROJ>_<TC>_<CONFIG>%s_O)\n' %
                     self.arch_ext)
    out += BUILD_RULES[self.tc][self.ptype.upper()][self.cfg] + '\n\n'
    return self.Replace(out)

  def BuildObjectList(self):
    obj_list = self.GetObjectList()
    sub_str = '$(patsubst %%,%s/%s/%%%s.o,$(%s_OBJS))' % (
        self.tc, self.cfg, self.arch_ext, self.project.upper())
    return '%s:=%s\n' % (obj_list, sub_str)

  def GetArches(self):
    return BUILD_RULES[self.tc]['ARCHES']

  def GetObjectList(self):
    return '%s_%s_%s%s_O' % (self.project.upper(), self.tc.upper(),
                              self.cfg.upper(), self.arch_ext)
  def GetPepperPlugin(self):
    plugin = self.Replace(BUILD_RULES[self.tc]['TOOL']['MAIN'])
    text = 'PPAPI_<CONFIG>:=$(abspath %s)' % plugin
    text += ';application/x-ppapi-%s\n' % self.vars['<config>'].lower()
    return self.Replace(text)

  def SetArch(self, arch):
    for key in arch:
      self.vars[key] = arch[key]
    if '<ARCH>' in self.vars:
      self.arch_ext = "_" + arch['<ARCH>']
      self.vars['<libdir>'] = "%s_%s" % (self.tc, self.vars['<ARCH>'])
    else:
      self.vars['<libdir>'] = self.tc

    toolset = arch.get('<TOOLSET>', self.tc.upper())
    self.SetTools(toolset)

  def SetConfig(self, config):
    self.cfg = config
    self.vars['<config>'] = config
    self.vars['<CONFIG>'] = config.upper()

  def SetDefines(self, defs):
    self.defines = defs
    self.vars['<DEFLIST>'] = self._BuildList('DEFINE', defs)

  def SetIncludes(self, incs):
    self.includes = incs
    self.vars['<INCLIST>'] = self._BuildList('INCLUDE', incs)

  def SetLibraries(self, libs):
    self.libraries = libs
    self.vars['<LIBLIST>'] = self._BuildList('LIBRARY', libs)

  def SetProject(self, proj, ptype, defs=None, incs=None, libs=None):
    self.project = proj
    self.ptype = ptype
    self.vars['<proj>'] = proj
    self.vars['<PROJ>'] = proj.upper()
    self.SetDefines(defs)
    self.SetIncludes(incs)
    self.SetLibraries(libs)

  def SetSource(self, src):
    self.vars['<src>'] = src

  def SetToolchain(self, tc):
    TC = tc.upper()
    if tc in ('linux', 'win'):
      tcname = 'host'
    else:
      tcname = tc
    self.vars['<tc>'] = tc
    self.vars['<tcname>'] = tcname
    self.vars['<TC>'] = TC
    self.SetDefines(self.defines)
    self.SetIncludes(self.includes)
    self.SetLibraries(self.libraries)

  def SetTools(self, toolname):
    self.vars['<CC>'] = '%s_CC' % toolname
    self.vars['<CXX>'] = '%s_CXX' % toolname
    self.vars['<LIB>'] = '%s_LIB' % toolname
    self.vars['<LINK>'] = '%s_LINK' % toolname

  def SetVars(self, **kwargs):
    # Add other passed in replacements
    for key in kwargs:
      self.vars['<%s>' % key] = kwargs[key]

  def Replace(self, text):
    return Replace(text, self.vars)


def Replace(text, replacements):
  for key in replacements:
    val = replacements[key]
    if val is not None:
      text = text.replace(key, val)
  return text


def SetVar(varname, values):
  if not values:
    return varname + ':=\n'

  line = varname + ':='
  out = ''
  for value in values:
    if len(line) + len(value) > 78:
      out += line[:-1] + '\n'
      line = '%s+=%s ' % (varname, value)
    else:
      line += value + ' '

  if line:
    out += line[:-1] + '\n'
  return out


def GenerateCleanRules(tools, configs):
  rules = '#\n# Target to remove temporary files\n#\n.PHONY: clean\nclean:\n'
  for tc in tools:
    for cfg in configs:
      rules += '\t$(RM) -fr %s/%s\n' % (tc, cfg)
  return rules + '\n'


def GenerateNMFRules(tc, main, dlls, cfg, arches):
  target = BUILD_RULES[tc]['TOOL']['NMFMAIN']
  dll_target = BUILD_RULES[tc]['TOOL']['SO']
  nmf_targets = []

  for arch in arches:
    replace = {
      '<config>' : cfg,
      '<TAB>' : '\t',
      '<tc>' : tc
    }
    if tc == 'glibc':
      replace['<OBJDUMP>'] = 'GLIBC_DUMP'
    if '<ARCH>' in arch:
      replace['<ARCH>'] = arch['<ARCH>']
    for dll in dlls:
      replace['<proj>'] = dll
      nmf_targets.append(Replace(dll_target, replace))
    replace['<proj>'] = main
    nmf_targets.append(Replace(target, replace))

  replace['<NMF_TARGETS>'] = ' '.join(nmf_targets)
  rules = Replace(BUILD_RULES[tc]['NMF'], replace)
  return '\nALL_TARGETS+=%s/%s/%s.nmf' % (tc, cfg, main) + rules + '\n'

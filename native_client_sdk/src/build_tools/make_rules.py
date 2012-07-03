#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


#
# Default macros for various platforms.
#
NEWLIB_DEFAULTS = """
NEWLIB_CC?=$(TC_PATH)/$(OSNAME)_x86_newlib/bin/i686-nacl-gcc -c
NEWLIB_CXX?=$(TC_PATH)/$(OSNAME)_x86_newlib/bin/i686-nacl-g++ -c -std=gnu++98 
NEWLIB_LINK?=$(TC_PATH)/$(OSNAME)_x86_newlib/bin/i686-nacl-g++ -Wl,-as-needed
NEWLIB_DUMP?=$(TC_PATH)/$(OSNAME)_x86_newlib/x86_64-nacl/bin/objdump
NEWLIB_CCFLAGS?=-O0 -g -pthread $(NACL_WARNINGS)
NEWLIB_LDFLAGS?=-g -lppapi
"""

GLIBC_DEFAULTS = """
GLIBC_CC?=$(TC_PATH)/$(OSNAME)_x86_glibc/bin/i686-nacl-gcc -c
GLIBC_CXX?=$(TC_PATH)/$(OSNAME)_x86_glibc/bin/i686-nacl-g++ -c -std=gnu++98 
GLIBC_LINK?=$(TC_PATH)/$(OSNAME)_x86_glibc/bin/i686-nacl-g++ -Wl,-as-needed
GLIBC_DUMP?=$(TC_PATH)/$(OSNAME)_x86_glibc/x86_64-nacl/bin/objdump
GLIBC_PATHS:=-L $(TC_PATH)/$(OSNAME)_x86_glibc/x86_64-nacl/lib32
GLIBC_PATHS+=-L $(TC_PATH)/$(OSNAME)_x86_glibc/x86_64-nacl/lib
GLIBC_CCFLAGS?=-O0 -g -pthread $(NACL_WARNINGS)
GLIBC_LDFLAGS?=-g -lppapi
"""

PNACL_DEFAULTS = """
PNACL_CC?=$(TC_PATH)/$(OSNAME)_x86_pnacl/newlib/bin/pnacl-clang -c
PNACL_CXX?=$(TC_PATH)/$(OSNAME)_x86_pnacl/newlib/bin/pnacl-clang++ -c -std=gnu++98 
PNACL_LINK?=$(TC_PATH)/$(OSNAME)_x86_pnacl/newlib/bin/pnacl-clang++
PNACL_DUMP?=$(TC_PATH)/$(OSNAME)_x86_pnacl/newlib/bin/objdump
PNACL_CCFLAGS?=-O0 -g -pthread $(NACL_WARNINGS)
PNACL_LDFLAGS?=-g -lppapi
TRANSLATE:=$(TC_PATH)/$(OSNAME)_x86_pnacl/newlib/bin/pnacl-translate
"""

WIN_DEFAULTS = """
WIN_CC?=cl.exe
WIN_CXX?=cl.exe
WIN_LINK?=link.exe
WIN_LIB?=lib.exe
WIN_CCFLAGS=/I$(NACL_SDK_ROOT)/include -D WIN32 -D _WIN32
WIN_LDFLAGS=/LIBPATH:$(NACL_SDK_ROOT)/lib/win_x86_32_host
"""

#
# Compile rules for various platforms.
#
NACL_CC_RULE = """
<OBJS>:=$(patsubst %.<ext>, <tc>/%_<ARCH>.o,$(<PROJ>_<EXT>))
$(<OBJS>) : <tc>/%_<ARCH>.o : %.<ext> $(THIS_MAKE) | <tc>
<TAB>$(<CC>) -o $@ $< <MACH> $(<PROJ>_<EXT>FLAGS) -DTCNAME=<tc> $(<TC>_CCFLAGS) <DEFLIST>
"""

SO_CC_RULE = """
<OBJS>:=$(patsubst %.<ext>, <tc>/%_<ARCH>.o,$(<PROJ>_<EXT>))
$(<OBJS>) : <tc>/%_<ARCH>.o : %.<ext> $(THIS_MAKE) | <tc>
<TAB>$(<CC>) -o $@ $< <MACH> -fPIC $(<PROJ>_<EXT>FLAGS) -DTCNAME=<tc> $(<TC>_CCFLAGS) <DEFLIST>
"""

WIN_CC_RULE = """
<OBJS>:=$(patsubst %.<ext>, <tc>/%.obj,$(<PROJ>_<EXT>))
$(<OBJS>) : <tc>/%.obj : %.<ext> $(THIS_MAKE) | <tc>
<TAB>$(<CC>) /Fo$@ /c $< -DTCNAME=host $(WIN_CCFLAGS) <DEFLIST>
"""

#
# Link rules for various platforms.
#
NEXE_LINK_RULE = """
<tc>/<proj>_<ARCH>.nexe : <OBJS>
<TAB>$(<LINK>) -o $@ $^ <MACH> $(<PROJ>_LDFLAGS) $(<TC>_LDFLAGS) <LIBLIST>
<TC>_NMF+=<tc>/<proj>_<ARCH>.nexe
"""

PEXE_LINK_RULE = """
<tc>/<proj>.pexe : <OBJS>
<TAB>$(<LINK>) -o $@ $^ $(<PROJ>_LDFLAGS) $(<TC>_LDFLAGS) <LIBLIST>

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
<TAB>$(<LINK>) -o $@ $^ <MACH> -shared $(<PROJ>_LDFLAGS) <LIBLIST>
GLIBC_REMAP+= -n <proj>_<ARCH>.so,<proj>.so
<TC>_NMF+=<tc>/<proj>_<ARCH>.so
"""

WIN_LIB_RULE = """
$(NACL_SDK_ROOT)/lib/win_<ARCH>_host/<proj>.lib : <OBJS>
<TAB>$(<LIB>) /OUT:$@ $^ $(<PROJ>_<EXT>FLAGS) <LIBLIST>
"""

WIN_LINK_RULE = """
win/<proj>.dll : <OBJS>
<TAB>$(<LINK>) /DLL /OUT:$@ $(<PROJ>_<EXT>FLAGS) /LIBPATH:$(NACL_SDK_ROOT)/lib $^ <LIBLIST> $(WIN_LDFLAGS)
<TC>_NMF+=<tc>/<proj>.dll

HOST_ARGS:=--register-pepper-plugins=$(abspath win/<proj>.dll);application/x-nacl
LAUNCH_HOST: CHECK_FOR_CHROME all
<TAB>$(CHROME_PATH) $(HOST_ARGS) "localhost:5103/index_win.html"
"""


NMF_RULE = """
<tc>/<proj>.nmf : $(<TC>_NMF)
<TAB>$(NMF) -D $(<DUMP>) -o $@ $^ -t <tc> -s <tc>
"""

NMF_EMPTY = """
<tc>/<proj>.nmf : $(<TC>_NMF) | <tc>
<TAB>echo {} > $@
"""

GLIBC_NMF_RULE = """
<tc>/<proj>.nmf : $(<TC>_NMF)
<TAB>$(NMF) -D $(<DUMP>) -o $@ $(GLIBC_PATHS) $^ -t <tc> -s <tc> $(<TC>_REMAP)
"""


EXT_MAP = {
  'c': 'CC',
  'cc': 'CXX'
}

WIN_TOOL = {
  'DEFINE': '-D%s',
  'LIBRARY': '%s.lib',
  'main': '<tc>/<proj>.dll',
  'nmf': '<tc>/<proj>.nmf',
  'so': None,
  'lib': '$(NACL_SDK_ROOT)/lib/win_<ARCH>_host/<proj>.lib',
}

NACL_TOOL = {
  'DEFINE': '-D%s',
  'LIBRARY': '-l%s',
  'main': '<tc>/<proj>_<ARCH>.nexe',
  'nmf': '<tc>/<proj>.nmf',
  'so': '<tc>/<proj>_<ARCH>.so',
  'lib': '$(NACL_SDK_ROOT)/lib/$(OSNAME)_<ARCH>_<tc>/lib<proj>.a',
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
WIN_32 = {
  '<arch>': '',
  '<ARCH>': 'x86_32',
  '<MACH>': '',
}


BUILD_RULES = {
  'newlib' : {
    'ARCHES': [NACL_X86_32, NACL_X86_64],
    'DEFS': NEWLIB_DEFAULTS,
    'CC' : NACL_CC_RULE,
    'CXX' : NACL_CC_RULE,
    'NMF' : NMF_RULE,
    'MAIN': NEXE_LINK_RULE,
    'LIB': None,
    'SO' : None,
    'TOOL': NACL_TOOL,
  },
  'glibc' : {
    'ARCHES': [NACL_X86_32, NACL_X86_64],
    'DEFS': GLIBC_DEFAULTS,
    'CC': NACL_CC_RULE,
    'CXX': NACL_CC_RULE,
    'NMF' : GLIBC_NMF_RULE,
    'MAIN': NEXE_LINK_RULE,
    'LIB': None,
    'SO': SO_LINK_RULE,
    'TOOL': NACL_TOOL,
  },
  'pnacl' : {
    'ARCHES': [NACL_PNACL],
    'DEFS': PNACL_DEFAULTS,
    'CC': NACL_CC_RULE,
    'CXX': NACL_CC_RULE,
    'nmf' : NMF_RULE,
    'MAIN': PEXE_LINK_RULE,
    'LIB': None,
    'SO': None,
    'TOOL': NACL_TOOL
  },
  'win' : {
    'ARCHES': [WIN_32],
    'DEFS': WIN_DEFAULTS,
    'CC': WIN_CC_RULE,
    'CXX': WIN_CC_RULE,
    'NMF' : NMF_EMPTY,
    'MAIN': WIN_LINK_RULE,
    'LIB': WIN_LIB_RULE,
    'SO': None,
    'TOOL': WIN_TOOL
  }
}


def GetBuildRule(tool, ext):
  return BUILD_RULES[tool][ext]


def BuildDefineList(tool, defs):
  pattern = BUILD_RULES[tool]['TOOL']['DEFINE']
  defines = [(pattern % name) for name in defs]
  return ' '.join(defines)


def BuildLibList(tool, libs):
  pattern = BUILD_RULES[tool]['TOOL']['LIBRARY']
  libraries = [(pattern % name) for name in libs]
  return ' '.join(libraries)


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
    '<LIB>': '%s_LIB' % TC,
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


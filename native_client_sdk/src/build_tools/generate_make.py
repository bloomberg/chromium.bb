# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import buildbot_common
from buildbot_common import ErrorExit
from easy_template import RunTemplateFileIfChanged
from build_paths import SCRIPT_DIR, SDK_EXAMPLE_DIR

def Trace(msg):
  if Trace.verbose:
    sys.stderr.write(str(msg) + '\n')
Trace.verbose = False


def ShouldProcessHTML(desc):
  dest = desc['DEST']
  return dest.startswith('examples') or dest.startswith('tests')


def GenerateSourceCopyList(desc):
  sources = []
  # Add sources for each target
  for target in desc['TARGETS']:
    sources.extend(target['SOURCES'])

  # And HTML and data files
  sources.extend(desc.get('DATA', []))

  if ShouldProcessHTML(desc):
    sources.append('common.js')

  return sources


def GetSourcesDict(sources):
  source_map = {}
  for key in ['.c', '.cc']:
    source_list = [fname for fname in sources if fname.endswith(key)]
    if source_list:
      source_map[key] = source_list
    else:
      source_map[key] = []
  return source_map


def GetProjectObjects(source_dict):
  object_list = []
  for key in ['.c', '.cc']:
    for src in source_dict[key]:
      object_list.append(os.path.splitext(src)[0])
  return object_list


def GetPlatforms(plat_list, plat_filter, first_toolchain):
  platforms = []
  for plat in plat_list:
    if plat in plat_filter:
      platforms.append(plat)

  if first_toolchain:
    return [platforms[0]]
  return platforms


def ErrorMsgFunc(text):
  sys.stderr.write(text + '\n')


def AddMakeBat(pepperdir, makepath):
  """Create a simple batch file to execute Make.

  Creates a simple batch file named make.bat for the Windows platform at the
  given path, pointing to the Make executable in the SDK."""

  makepath = os.path.abspath(makepath)
  if not makepath.startswith(pepperdir):
    ErrorExit('Make.bat not relative to Pepper directory: ' + makepath)

  makeexe = os.path.abspath(os.path.join(pepperdir, 'tools'))
  relpath = os.path.relpath(makeexe, makepath)

  fp = open(os.path.join(makepath, 'make.bat'), 'wb')
  outpath = os.path.join(relpath, 'make.exe')

  # Since make.bat is only used by Windows, for Windows path style
  outpath = outpath.replace(os.path.sep, '\\')
  fp.write('@%s %%*\n' % outpath)
  fp.close()


def FindFile(name, srcroot, srcdirs):
  checks = []
  for srcdir in srcdirs:
    srcfile = os.path.join(srcroot, srcdir, name)
    srcfile = os.path.abspath(srcfile)
    if os.path.exists(srcfile):
      return srcfile
    else:
      checks.append(srcfile)

  ErrorMsgFunc('%s not found in:\n\t%s' % (name, '\n\t'.join(checks)))
  return None


def IsNexe(desc):
  for target in desc['TARGETS']:
    if target['TYPE'] == 'main':
      return True
  return False


def ProcessHTML(srcroot, dstroot, desc, toolchains, configs, first_toolchain):
  name = desc['NAME']
  nmf = desc['TARGETS'][0]['NAME']
  outdir = os.path.join(dstroot, desc['DEST'], name)
  srcpath = os.path.join(srcroot, 'index.html')
  dstpath = os.path.join(outdir, 'index.html')
  
  tools = GetPlatforms(toolchains, desc['TOOLS'], first_toolchain)

  path = "{tc}/{config}"
  replace = {
    'title': desc['TITLE'],
    'attrs':
        'data-name="%s" data-tools="%s" data-configs="%s" data-path="%s"' % (
        nmf, ' '.join(tools), ' '.join(configs), path),
  }
  RunTemplateFileIfChanged(srcpath, dstpath, replace)


def FindAndCopyFiles(src_files, root, search_dirs, dst_dir):
  buildbot_common.MakeDir(dst_dir)
  for src_name in src_files:
    src_file = FindFile(src_name, root, search_dirs)
    if not src_file:
      ErrorExit('Failed to find: ' + src_name)
    dst_file = os.path.join(dst_dir, src_name)
    if os.path.exists(dst_file):
      if os.stat(src_file).st_mtime <= os.stat(dst_file).st_mtime:
        Trace('Skipping "%s", destination "%s" is newer.' % (
            src_file, dst_file))
        continue
    buildbot_common.CopyFile(src_file, dst_file)


def ProcessProject(srcroot, dstroot, desc, toolchains, configs=None,
                   first_toolchain=False):
  if not configs:
    configs = ['Debug', 'Release']

  name = desc['NAME']
  out_dir = os.path.join(dstroot, desc['DEST'], name)
  buildbot_common.MakeDir(out_dir)
  srcdirs = desc.get('SEARCH', ['.', '..', '../..'])

  # Copy sources to example directory
  sources = GenerateSourceCopyList(desc)
  FindAndCopyFiles(sources, srcroot, srcdirs, out_dir)

  # Copy public headers to the include directory.
  for headers_set in desc.get('HEADERS', []):
    headers = headers_set['FILES']
    header_out_dir = os.path.join(dstroot, headers_set['DEST'])
    FindAndCopyFiles(headers, srcroot, srcdirs, header_out_dir)

  make_path = os.path.join(out_dir, 'Makefile')

  if IsNexe(desc):
    template = os.path.join(SCRIPT_DIR, 'template.mk')
  else:
    template = os.path.join(SCRIPT_DIR, 'library.mk')

  # Ensure the order of |tools| is the same as toolchains; that way if
  # first_toolchain is set, it will choose based on the order of |toolchains|.
  tools = [tool for tool in toolchains if tool in desc['TOOLS']]
  if first_toolchain:
    tools = [tools[0]]
  template_dict = {
    'rel_sdk': '/'.join(['..'] * (len(desc['DEST'].split('/')) + 1)),
    'pre': desc.get('PRE', ''),
    'post': desc.get('POST', ''),
    'tools': tools,
    'targets': desc['TARGETS'],
  }
  RunTemplateFileIfChanged(template, make_path, template_dict)

  outdir = os.path.dirname(os.path.abspath(make_path))
  pepperdir = os.path.dirname(os.path.dirname(outdir))
  AddMakeBat(pepperdir, outdir)

  if ShouldProcessHTML(desc):
    ProcessHTML(srcroot, dstroot, desc, toolchains, configs,
                first_toolchain)

  return (name, desc['DEST'])


def GenerateMasterMakefile(out_path, targets, depth):
  """Generate a Master Makefile that builds all examples.

  Args:
    out_path: Root for output such that out_path+NAME = full path
    targets: List of targets names
    depth: How deep in from NACL_SDK_ROOT
  """
  in_path = os.path.join(SDK_EXAMPLE_DIR, 'Makefile')
  out_path = os.path.join(out_path, 'Makefile')
  template_dict = {
    'projects': targets,
    'rel_sdk' : '/'.join(['..'] * depth)
  }
  RunTemplateFileIfChanged(in_path, out_path, template_dict)
  outdir = os.path.dirname(os.path.abspath(out_path))
  pepperdir = os.path.dirname(outdir)
  AddMakeBat(pepperdir, outdir)



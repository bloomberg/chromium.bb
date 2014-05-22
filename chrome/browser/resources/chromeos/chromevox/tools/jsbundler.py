#!/usr/bin/env python

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Produces various output formats from a set of JavaScript files with
closure style require/provide calls.

Scans one or more directory trees for JavaScript files.  Then, from a
given list of top-level files, sorts all required input files topologically.
The top-level files are appended to the sorted list in the order specified
on the command line.  If no root directories are specified, the source
files are assumed to be ordered already and no dependency analysis is
performed.  The resulting file list can then be used in one of the following
ways:

- list: a plain list of files, one per line is output.

- html: a series of html <script> tags with src attributes containing paths
  is output.

- bundle: a concatenation of all the files, separated by newlines is output.

- compressed_bundle: A bundle where non-significant whitespace, including
  comments, has been stripped is output.

- copy: the files are copied, or hard linked if possible, to the destination
  directory.  In this case, no output is generated.
'''


import optparse
import os
import shutil
import sys

_SCRIPT_DIR = os.path.realpath(os.path.dirname(__file__))
_CHROME_SOURCE = os.path.realpath(
    os.path.join(_SCRIPT_DIR, *[os.path.pardir] * 6))
sys.path.insert(0, os.path.join(
    _CHROME_SOURCE, 'third_party/WebKit/Source/build/scripts'))
sys.path.insert(0, os.path.join(
    _CHROME_SOURCE, ('chrome/third_party/chromevox/third_party/' +
                     'closure-library/closure/bin/build')))
import depstree
import rjsmin
import source
import treescan


def Die(message):
  '''Prints an error message and exit the program.'''
  print >>sys.stderr, message
  sys.exit(1)


class SourceWithPaths(source.Source):
  '''A source.Source object with its relative input and output paths'''

  def __init__(self, content, in_path, out_path):
    super(SourceWithPaths, self).__init__(content)
    self._in_path = in_path
    self._out_path = out_path

  def GetInPath(self):
    return self._in_path

  def GetOutPath(self):
    return self._out_path


class Bundle():
  '''An ordered list of sources without duplicates.'''

  def __init__(self):
    self._added_paths = set()
    self._added_sources = []

  def Add(self, sources):
    '''Appends one or more source objects the list if it doesn't already
    exist.

    Args:
      sources: A SourceWithPath or an iterable of such objects.
    '''
    if isinstance(sources, SourceWithPaths):
      sources = [sources]
    for source in sources:
      path = source.GetInPath()
      if path not in self._added_paths:
        self._added_paths.add(path)
        self._added_sources.append(source)

  def GetOutPaths(self):
    return (source.GetOutPath() for source in self._added_sources)

  def GetSources(self):
    return self._added_sources

  def GetUncompressedSource(self):
    return '\n'.join((s.GetSource() for s in self._added_sources))

  def GetCompressedSource(self):
    return rjsmin.jsmin(self.GetUncompressedSource())


class PathRewriter():
  '''A list of simple path rewrite rules to map relative input paths to
  relative output paths.
  '''

  def __init__(self, specs):
    '''Args:
      specs: A list of mappings, each consisting of the input prefix and
        the corresponding output prefix separated by colons.
    '''
    self._prefix_map = []
    for spec in specs:
      parts = spec.split(':')
      if len(parts) != 2:
        Die('Invalid prefix rewrite spec %s' % spec)
      if not parts[0].endswith('/'):
        parts[0] += '/'
      self._prefix_map.append(parts)

  def RewritePath(self, in_path):
    '''Rewrites an input path according to the list of rules.

    Args:
      in_path, str: The input path to rewrite.
    Returns:
      str: The corresponding output path.
    '''
    for in_prefix, out_prefix in self._prefix_map:
      if in_path.startswith(in_prefix):
        return os.path.join(out_prefix, in_path[len(in_prefix):])
    return in_path


def ReadSources(options, args):
  '''Reads all source specified on the command line, including sources
  included by --root options.
  '''

  def EnsureSourceLoaded(in_path, sources, path_rewriter):
    if in_path not in sources:
      out_path = path_rewriter.RewritePath(in_path)
      sources[in_path] = SourceWithPaths(source.GetFileContents(in_path),
                                         in_path, out_path)

  # Only read the actual source file if we will do a dependency analysis or
  # if we'll need it for the output.
  need_source_text = (len(options.roots) > 0 or
                      options.mode in ('bundle', 'compressed_bundle'))
  path_rewriter = PathRewriter(options.prefix_map)
  sources = {}
  for root in options.roots:
    for name in treescan.ScanTreeForJsFiles(root):
      EnsureSourceLoaded(name, sources, path_rewriter)
  for path in args:
    if need_source_text:
      EnsureSourceLoaded(path, sources, path_rewriter)
    else:
      # Just add an empty representation of the source.
      sources[path] = SourceWithPaths(
          '', path, path_rewriter.RewritePath(path))
  return sources


def CalcDeps(bundle, sources, top_level):
  '''Calculates dependencies for a set of top-level files.

  Args:
    bundle: Bundle to add the sources to.
    sources, dict: Mapping from input path to SourceWithPaths objects.
    top_level, list: List of top-level input paths to calculate dependencies
      for.
  '''
  def GetBase(sources):
    for source in sources.itervalues():
      if (os.path.basename(source.GetInPath()) == 'base.js' and
          'goog' in source.provides):
        return source
    Die('goog.base not provided by any file')

  providers = [s for s in sources.itervalues() if len(s.provides) > 0]
  deps = depstree.DepsTree(providers)
  namespaces = []
  for path in top_level:
    namespaces.extend(sources[path].requires)
  # base.js is an implicit dependency that always goes first.
  bundle.Add(GetBase(sources))
  bundle.Add(deps.GetDependencies(namespaces))


def LinkOrCopyFiles(sources, dest_dir):
  '''Copies a list of sources to a destination directory.'''

  def LinkOrCopyOneFile(src, dst):
    if not os.path.exists(os.path.dirname(dst)):
      os.makedirs(os.path.dirname(dst))
    if os.path.exists(dst):
      # Avoid clobbering the inode if source and destination refer to the
      # same file already.
      if os.path.samefile(src, dst):
        return
      os.unlink(dst)
    try:
      os.link(src, dst)
    except:
      shutil.copy(src, dst)

  for source in sources:
    LinkOrCopyOneFile(source.GetInPath(),
                      os.path.join(dest_dir, source.GetOutPath()))


def WriteOutput(bundle, format, out_file, dest_dir):
  '''Writes output in the specified format.

  Args:
    bundle: The ordered bundle iwth all sources already added.
    format: Output format, one of list, html, bundle, compressed_bundle.
    out_file: File object to receive the output.
    dest_dir: Prepended to each path mentioned in the output, if applicable.
  '''
  if format == 'list':
    paths = bundle.GetOutPaths()
    if dest_dir:
      paths = (os.path.join(dest_dir, p) for p in paths)
    paths = (os.path.normpath(p) for p in paths)
    out_file.write('\n'.join(paths))
  elif format == 'html':
    HTML_TEMPLATE = '<script src=\'%s\'>'
    script_lines = (HTML_TEMPLATE % p for p in bundle.GetOutPaths())
    out_file.write('\n'.join(script_lines))
  elif format == 'bundle':
    out_file.write(bundle.GetUncompressedSource())
  elif format == 'compressed_bundle':
    out_file.write(bundle.GetCompressedSource())
  out_file.write('\n')


def CreateOptionParser():
  parser = optparse.OptionParser(description=__doc__)
  parser.usage = '%prog [options] <top_level_file>...'
  parser.add_option('-d', '--dest_dir', action='store', metavar='DIR',
                    help=('Destination directory.  Used when translating ' +
                          'input paths to output paths and when copying '
                          'files.'))
  parser.add_option('-o', '--output_file', action='store', metavar='FILE',
                    help=('File to output result to for modes that output '
                          'a single file.'))
  parser.add_option('-r', '--root', dest='roots', action='append', default=[],
                    metavar='ROOT',
                    help='Roots of directory trees to scan for sources.')
  parser.add_option('-w', '--rewrite_prefix', action='append', default=[],
                    dest='prefix_map', metavar='SPEC',
                    help=('Two path prefixes, separated by colons ' +
                          'specifying that a file whose (relative) path ' +
                          'name starts with the first prefix should have ' +
                          'that prefix replaced by the second prefix to ' +
                          'form a path relative to the output directory.'))
  parser.add_option('-m', '--mode', type='choice', action='store',
                    choices=['list', 'html', 'bundle',
                             'compressed_bundle', 'copy'],
                    default='list', metavar='MODE',
                    help=("Otput mode. One of 'list', 'html', 'bundle', " +
                          "'compressed_bundle' or 'copy'."))
  return parser


def main():
  options, args = CreateOptionParser().parse_args()
  if len(args) < 1:
    Die('At least one top-level source file must be specified.')
  sources = ReadSources(options, args)
  bundle = Bundle()
  if len(options.roots) > 0:
    CalcDeps(bundle, sources, args)
  bundle.Add((sources[name] for name in args))
  if options.mode == 'copy':
    if options.dest_dir is None:
      Die('Must specify --dest_dir when copying.')
    LinkOrCopyFiles(bundle.GetSources(), options.dest_dir)
  else:
    if options.output_file:
      out_file = open(options.output_file, 'w')
    else:
      out_file = sys.stdout
    try:
      WriteOutput(bundle, options.mode, out_file, options.dest_dir)
    finally:
      if options.output_file:
        out_file.close()


if __name__ == '__main__':
  main()

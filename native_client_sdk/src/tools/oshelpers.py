#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import glob
import optparse
import os
import posixpath
import shutil
import stat
import sys
import time
import zipfile

if sys.version_info < (2, 6, 0):
  sys.stderr.write("python 2.6 or later is required run this script\n")
  sys.exit(1)


def IncludeFiles(filters, files):
  """Filter files based on inclusion lists

  Return a list of files which match and of the Unix shell-style wildcards
  provided, or return all the files if no filter is provided."""
  if not filters:
    return files
  match = set()
  for file_filter in filters:
    match |= set(fnmatch.filter(files, file_filter))
  return [name for name in files if name in match]


def ExcludeFiles(filters, files):
  """Filter files based on exclusions lists

  Return a list of files which do not match any of the Unix shell-style
  wildcards provided, or return all the files if no filter is provided."""
  if not filters:
    return files
  match = set()
  for file_filter in filters:
    excludes = set(fnmatch.filter(files, file_filter))
    match |= excludes
  return [name for name in files if name not in match]


def CopyPath(options, src, dst):
  """CopyPath from src to dst

  Copy a fully specified src to a fully specified dst.  If src and dst are
  both files, the dst file is removed first to prevent error.  If and include
  or exclude list are provided, the destination is first matched against that
  filter."""
  if options.includes:
    if not IncludeFiles(options.includes, [src]):
      return

  if options.excludes:
    if not ExcludeFiles(options.excludes, [src]):
      return

  if options.verbose:
    print 'cp %s %s' % (src, dst)

  # If the source is a single file, copy it individually
  if os.path.isfile(src):
    # We can not copy over a directory with a file.
    if os.path.exists(dst):
      if not os.path.isfile(dst):
        msg = "cp: cannot overwrite non-file '%s' with file." % dst
        raise OSError(msg)
      # If the destination exists as a file, remove it before copying to avoid
      # 'readonly' issues.
      os.remove(dst)

    # Now copy to the non-existent fully qualified target
    shutil.copy(src, dst)
    return

  # Otherwise it's a directory, ignore it unless allowed
  if os.path.isdir(src):
    if not options.recursive:
      print "cp: omitting directory '%s'" % src
      return

    # We can not copy over a file with a directory.
    if os.path.exists(dst):
      if not os.path.isdir(dst):
        msg = "cp: cannot overwrite non-directory '%s' with directory." % dst
        raise OSError(msg)
    else:
      # if it didn't exist, create the directory
      os.makedirs(dst)

    # Now copy all members
    for filename in os.listdir(src):
      srcfile = os.path.join(src, filename)
      dstfile = os.path.join(dst, filename)
      CopyPath(options, srcfile, dstfile)
  return


def Copy(args):
  """A Unix cp style copy.

  Copies multiple sources to a single destination using the normal cp
  semantics.  In addition, it support inclusion and exclusion filters which
  allows the copy to skip certain types of files."""
  parser = optparse.OptionParser(usage='usage: cp [Options] sources... dest')
  parser.add_option(
      '-R', '-r', '--recursive', dest='recursive', action='store_true',
      default=False,
      help='copy directories recursively.')
  parser.add_option(
      '-v', '--verbose', dest='verbose', action='store_true',
      default=False,
      help='verbose output.')
  parser.add_option(
      '--include', dest='includes', action='append', default=[],
      help='include files matching this expression.')
  parser.add_option(
      '--exclude', dest='excludes', action='append', default=[],
      help='exclude files matching this expression.')
  options, files = parser.parse_args(args)
  if len(files) < 2:
    parser.error('ERROR: expecting SOURCE(s) and DEST.')

  srcs = files[:-1]
  dst = files[-1]

  src_list = []
  for src in srcs:
    files = glob.glob(src)
    if not files:
      raise OSError('cp: no such file or directory: ' + src)
    if files:
      src_list.extend(files)

  for src in src_list:
    # If the destination is a directory, then append the basename of the src
    # to the destination.
    if os.path.isdir(dst):
      CopyPath(options, src, os.path.join(dst, os.path.basename(src)))
    else:
      CopyPath(options, src, dst)


def Mkdir(args):
  """A Unix style mkdir"""
  parser = optparse.OptionParser(usage='usage: mkdir [Options] DIRECTORY...')
  parser.add_option(
      '-p', '--parents', dest='parents', action='store_true',
      default=False,
      help='ignore existing parents, create parents as needed.')
  parser.add_option(
      '-v', '--verbose', dest='verbose', action='store_true',
      default=False,
      help='verbose output.')

  options, dsts = parser.parse_args(args)
  if len(dsts) < 1:
    parser.error('ERROR: expecting DIRECTORY...')

  for dst in dsts:
    if options.verbose:
      print 'mkdir ' + dst
    try:
      os.makedirs(dst)
    except OSError:
      if os.path.isdir(dst):
        if options.parents:
          continue
        raise OSError('mkdir: Already exists: ' + dst)
      else:
        raise OSError('mkdir: Failed to create: ' + dst)
  return 0


def MovePath(options, src, dst):
  """MovePath from src to dst

  Moves the src to the dst much like the Unix style mv command, except it
  only handles one source at a time.  Because of possible temporary failures
  do to locks (such as anti-virus software on Windows), the function will retry
  up to five times."""
  # if the destination is not an existing directory, then overwrite it
  if os.path.isdir(dst):
    dst = os.path.join(dst, os.path.basename(src))

  # If the destination exists, the remove it
  if os.path.exists(dst):
    if options.force:
      Remove(['-vfr', dst])
      if os.path.exists(dst):
        raise OSError('mv: FAILED TO REMOVE ' + dst)
    else:
      raise OSError('mv: already exists ' + dst)
  for _ in range(5):
    try:
      os.rename(src, dst)
      return
    except OSError as error:
      print 'Failed on %s with %s, retrying' % (src, error)
      time.sleep(5)
  print 'Gave up.'
  raise OSError('mv: ' + error)


def Move(args):
  parser = optparse.OptionParser(usage='usage: mv [Options] sources... dest')
  parser.add_option(
      '-v', '--verbose', dest='verbose', action='store_true',
      default=False,
      help='verbose output.')
  parser.add_option(
      '-f', '--force', dest='force', action='store_true',
      default=False,
      help='force, do not error it files already exist.')
  options, files = parser.parse_args(args)
  if len(files) < 2:
    parser.error('ERROR: expecting SOURCE... and DEST.')

  srcs = files[:-1]
  dst = files[-1]

  if options.verbose:
    print 'mv %s %s' % (' '.join(srcs), dst)

  for src in srcs:
    MovePath(options, src, dst)
  return 0


def Remove(args):
  """A Unix style rm.

  Removes the list of paths.  Because of possible temporary failures do to locks
  (such as anti-virus software on Windows), the function will retry up to five
  times."""
  parser = optparse.OptionParser(usage='usage: rm [Options] PATHS...')
  parser.add_option(
      '-R', '-r', '--recursive', dest='recursive', action='store_true',
      default=False,
      help='remove directories recursively.')
  parser.add_option(
      '-v', '--verbose', dest='verbose', action='store_true',
      default=False,
      help='verbose output.')
  parser.add_option(
      '-f', '--force', dest='force', action='store_true',
      default=False,
      help='force, do not error it files does not exist.')
  options, files = parser.parse_args(args)
  if len(files) < 1:
    parser.error('ERROR: expecting FILE...')

  try:
    for pattern in files:
      dst_files = glob.glob(pattern)
      if not dst_files:
        # Ignore non existing files when using force
        if options.force:
          continue
        raise OSError('rm: no such file or directory: ' + pattern)

      for dst in dst_files:
        if options.verbose:
          print 'rm ' + dst

        if os.path.isfile(dst) or os.path.islink(dst):
          for i in range(5):
            try:
              # Check every time, since it may have been deleted after the
              # previous failed attempt.
              if os.path.isfile(dst) or os.path.islink(dst):
                os.remove(dst)
              break
            except OSError as error:
              if i == 5:
                print 'Gave up.'
                raise OSError('rm: ' + str(error))
              print 'Failed remove with %s, retrying' % error
              time.sleep(5)

        if options.recursive:
          for i in range(5):
            try:
              if os.path.isdir(dst):
                shutil.rmtree(dst)
              break
            except OSError as error:
              if i == 5:
                print 'Gave up.'
                raise OSError('rm: ' + str(error))
              print 'Failed rmtree with %s, retrying' % error
              time.sleep(5)

  except OSError as error:
    print error
  return 0


def MakeZipPath(os_path, isdir, iswindows):
  """Changes a path into zipfile format.

  # doctest doesn't seem to honor r'' strings, so the backslashes need to be
  # escaped.
  >>> MakeZipPath(r'C:\\users\\foobar\\blah', False, True)
  'users/foobar/blah'
  >>> MakeZipPath('/tmp/tmpfoobar/something', False, False)
  'tmp/tmpfoobar/something'
  >>> MakeZipPath('./somefile.txt', False, False)
  'somefile.txt'
  >>> MakeZipPath('somedir', True, False)
  'somedir/'
  >>> MakeZipPath('../dir/filename.txt', False, False)
  '../dir/filename.txt'
  >>> MakeZipPath('dir/../filename.txt', False, False)
  'filename.txt'
  """
  zip_path = os_path
  if iswindows:
    import ntpath
    # zipfile paths are always posix-style. They also have the drive
    # letter and leading slashes removed.
    zip_path = ntpath.splitdrive(os_path)[1].replace('\\', '/')
  if zip_path.startswith('/'):
    zip_path = zip_path[1:]
  zip_path = posixpath.normpath(zip_path)
  # zipfile also always appends a slash to a directory name.
  if isdir:
    zip_path += '/'
  return zip_path


def OSMakeZipPath(os_path):
  return MakeZipPath(os_path, os.path.isdir(os_path), sys.platform == 'win32')


def Zip(args):
  """A Unix style zip.

  Compresses the listed files."""
  parser = optparse.OptionParser(usage='usage: zip [Options] zipfile list')
  parser.add_option(
      '-r', dest='recursive', action='store_true',
      default=False,
      help='recurse into directories')
  parser.add_option(
      '-q', dest='quiet', action='store_true',
      default=False,
      help='quiet operation')
  options, files = parser.parse_args(args)
  if len(files) < 2:
    parser.error('ERROR: expecting ZIPFILE and LIST.')

  dest_zip = files[0]
  src_args = files[1:]

  src_files = []
  for src_arg in src_args:
    globbed_src_args = glob.glob(src_arg)
    if not globbed_src_args:
      if not options.quiet:
        print 'zip warning: name not matched: %s' % (src_arg,)

    for src_file in globbed_src_args:
      src_file = os.path.normpath(src_file)
      src_files.append(src_file)
      if options.recursive and os.path.isdir(src_file):
        for root, dirs, files in os.walk(src_file):
          for dirname in dirs:
            src_files.append(os.path.join(root, dirname))
          for filename in files:
            src_files.append(os.path.join(root, filename))

  zip_stream = None
  # zip_data represents a list of the data to be written or appended to the
  # zip_stream. It is a list of tuples:
  #   (OS file path, zip path/zip file info, and file data)
  # In all cases one of the |os path| or the |file data| will be None.
  # |os path| is None when there is no OS file to write to the archive (i.e.
  # the file data already existed in the archive). |file data| is None when the
  # file is new (never existed in the archive) or being updated.
  zip_data = []
  new_files_to_add = [OSMakeZipPath(src_file) for src_file in src_files]
  zip_path_to_os_path_dict = dict((new_files_to_add[i], src_files[i])
                                  for i in range(len(src_files)))
  write_mode = 'a'
  try:
    zip_stream = zipfile.ZipFile(dest_zip, 'r')
    files_to_update = set(new_files_to_add).intersection(
        set(zip_stream.namelist()))
    if files_to_update:
      # As far as I can tell, there is no way to update a zip entry using
      # zipfile; the best you can do is rewrite the archive.
      # Iterate through the zipfile to maintain file order.
      write_mode = 'w'
      for zip_path in zip_stream.namelist():
        if zip_path in files_to_update:
          os_path = zip_path_to_os_path_dict[zip_path]
          zip_data.append((os_path, zip_path, None))
          new_files_to_add.remove(zip_path)
        else:
          file_bytes = zip_stream.read(zip_path)
          file_info = zip_stream.getinfo(zip_path)
          zip_data.append((None, file_info, file_bytes))
  except IOError:
    pass
  finally:
    if zip_stream:
      zip_stream.close()

  for zip_path in new_files_to_add:
    zip_data.append((zip_path_to_os_path_dict[zip_path], zip_path, None))

  if not zip_data:
    print 'zip error: Nothing to do! (%s)' % (dest_zip,)
    return 1

  try:
    zip_stream = zipfile.ZipFile(dest_zip, write_mode, zipfile.ZIP_DEFLATED)
    for os_path, file_info_or_zip_path, file_bytes in zip_data:
      if isinstance(file_info_or_zip_path, zipfile.ZipInfo):
        zip_path = file_info_or_zip_path.filename
      else:
        zip_path = file_info_or_zip_path

      if os_path:
        st = os.stat(os_path)
        if stat.S_ISDIR(st.st_mode):
          # Python 2.6 on the buildbots doesn't support writing directories to
          # zip files. This was resolved in a later version of Python 2.6.
          # We'll work around it by writing an empty file with the correct
          # path. (This is basically what later versions do anyway.)
          zip_info = zipfile.ZipInfo()
          zip_info.filename = zip_path
          zip_info.date_time = time.localtime(st.st_mtime)[0:6]
          zip_info.compress_type = zip_stream.compression
          zip_info.flag_bits = 0x00
          zip_info.external_attr = (st[0] & 0xFFFF) << 16L
          zip_info.CRC = 0
          zip_info.compress_size = 0
          zip_info.file_size = 0
          zip_stream.writestr(zip_info, '')
        else:
          zip_stream.write(os_path, zip_path)
      else:
        zip_stream.writestr(file_info_or_zip_path, file_bytes)

      if not options.quiet:
        if zip_path in new_files_to_add:
          operation = 'adding'
        else:
          operation = 'updating'
        zip_info = zip_stream.getinfo(zip_path)
        if (zip_info.compress_type == zipfile.ZIP_STORED or
            zip_info.file_size == 0):
          print '  %s: %s (stored 0%%)' % (operation, zip_path)
        elif zip_info.compress_type == zipfile.ZIP_DEFLATED:
          print '  %s: %s (deflated %d%%)' % (operation, zip_path,
              100 - zip_info.compress_size * 100 / zip_info.file_size)
  finally:
    zip_stream.close()

  return 0


def FindExeInPath(filename):
  env_path = os.environ.get('PATH', '')
  paths = env_path.split(os.pathsep)

  def IsExecutableFile(path):
    return os.path.isfile(path) and os.access(path, os.X_OK)

  if os.path.sep in filename:
    if IsExecutableFile(filename):
      return filename

  for path in paths:
    filepath = os.path.join(path, filename)
    if IsExecutableFile(filepath):
      return os.path.abspath(os.path.join(path, filename))


def Which(args):
  """A Unix style which.

  Looks for all arguments in the PATH environment variable, and prints their
  path if they are executable files.

  Note: If you pass an argument with a path to which, it will just test if it
  is executable, not if it is in the path.
  """
  parser = optparse.OptionParser(usage='usage: which args...')
  _, files = parser.parse_args(args)
  if not files:
    return 0

  retval = 0
  for filename in files:
    fullname = FindExeInPath(filename)
    if fullname:
      print fullname
    else:
      retval = 1

  return retval


FuncMap = {
  'cp': Copy,
  'mkdir': Mkdir,
  'mv': Move,
  'rm': Remove,
  'zip': Zip,
  'which': Which,
}


def main(args):
  if not args:
    print 'No command specified'
    print 'Available commands: %s' % ' '.join(FuncMap)
    return 1
  func_name = args[0]
  func = FuncMap.get(func_name)
  if not func:
    print 'Do not recognize command: %s' % func_name
    print 'Available commands: %s' % ' '.join(FuncMap)
    return 1
  try:
    return func(args[1:])
  except KeyboardInterrupt:
    print '%s: interrupted' % func_name
    return 1

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

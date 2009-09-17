# Copyright 2009 Google Inc.  All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


import os
import re
import subprocess
import sys
import xml.dom.minidom

import gclient_utils

SVN_COMMAND = "svn"


### SCM abstraction layer


class SCMWrapper(object):
  """Add necessary glue between all the supported SCM.

  This is the abstraction layer to bind to different SCM. Since currently only
  subversion is supported, a lot of subersionism remains. This can be sorted out
  once another SCM is supported."""
  def __init__(self, url=None, root_dir=None, relpath=None,
               scm_name='svn'):
    # TODO(maruel): Deduce the SCM from the url.
    self.scm_name = scm_name
    self.url = url
    self._root_dir = root_dir
    if self._root_dir:
      self._root_dir = self._root_dir.replace('/', os.sep)
    self.relpath = relpath
    if self.relpath:
      self.relpath = self.relpath.replace('/', os.sep)

  def FullUrlForRelativeUrl(self, url):
    # Find the forth '/' and strip from there. A bit hackish.
    return '/'.join(self.url.split('/')[:4]) + url

  def RunCommand(self, command, options, args, file_list=None):
    # file_list will have all files that are modified appended to it.
    if file_list is None:
      file_list = []

    commands = {
      'cleanup':  self.cleanup,
      'export':   self.export,
      'update':   self.update,
      'revert':   self.revert,
      'status':   self.status,
      'diff':     self.diff,
      'pack':     self.pack,
      'runhooks': self.status,
    }

    if not command in commands:
      raise gclient_utils.Error('Unknown command %s' % command)

    return commands[command](options, args, file_list)

  def cleanup(self, options, args, file_list):
    """Cleanup working copy."""
    command = ['cleanup']
    command.extend(args)
    RunSVN(command, os.path.join(self._root_dir, self.relpath))

  def diff(self, options, args, file_list):
    # NOTE: This function does not currently modify file_list.
    command = ['diff']
    command.extend(args)
    RunSVN(command, os.path.join(self._root_dir, self.relpath))

  def export(self, options, args, file_list):
    assert len(args) == 1
    export_path = os.path.abspath(os.path.join(args[0], self.relpath))
    try:
      os.makedirs(export_path)
    except OSError:
      pass
    assert os.path.exists(export_path)
    command = ['export', '--force', '.']
    command.append(export_path)
    RunSVN(command, os.path.join(self._root_dir, self.relpath))

  def update(self, options, args, file_list):
    """Runs SCM to update or transparently checkout the working copy.

    All updated files will be appended to file_list.

    Raises:
      Error: if can't get URL for relative path.
    """
    # Only update if git is not controlling the directory.
    checkout_path = os.path.join(self._root_dir, self.relpath)
    git_path = os.path.join(self._root_dir, self.relpath, '.git')
    if os.path.exists(git_path):
      print("________ found .git directory; skipping %s" % self.relpath)
      return

    if args:
      raise gclient_utils.Error("Unsupported argument(s): %s" % ",".join(args))

    url = self.url
    components = url.split("@")
    revision = None
    forced_revision = False
    if options.revision:
      # Override the revision number.
      url = '%s@%s' % (components[0], str(options.revision))
      revision = int(options.revision)
      forced_revision = True
    elif len(components) == 2:
      revision = int(components[1])
      forced_revision = True

    rev_str = ""
    if revision:
      rev_str = ' at %d' % revision

    if not os.path.exists(checkout_path):
      # We need to checkout.
      command = ['checkout', url, checkout_path]
      if revision:
        command.extend(['--revision', str(revision)])
      RunSVNAndGetFileList(command, self._root_dir, file_list)
      return

    # Get the existing scm url and the revision number of the current checkout.
    from_info = CaptureSVNInfo(os.path.join(checkout_path, '.'), '.')
    if not from_info:
      raise gclient_utils.Error("Can't update/checkout %r if an unversioned "
                                "directory is present. Delete the directory "
                                "and try again." %
                                checkout_path)

    if options.manually_grab_svn_rev:
      # Retrieve the current HEAD version because svn is slow at null updates.
      if not revision:
        from_info_live = CaptureSVNInfo(from_info['URL'], '.')
        revision = int(from_info_live['Revision'])
        rev_str = ' at %d' % revision

    if from_info['URL'] != components[0]:
      to_info = CaptureSVNInfo(url, '.')
      can_switch = ((from_info['Repository Root'] != to_info['Repository Root'])
                    and (from_info['UUID'] == to_info['UUID']))
      if can_switch:
        print("\n_____ relocating %s to a new checkout" % self.relpath)
        # We have different roots, so check if we can switch --relocate.
        # Subversion only permits this if the repository UUIDs match.
        # Perform the switch --relocate, then rewrite the from_url
        # to reflect where we "are now."  (This is the same way that
        # Subversion itself handles the metadata when switch --relocate
        # is used.)  This makes the checks below for whether we
        # can update to a revision or have to switch to a different
        # branch work as expected.
        # TODO(maruel):  TEST ME !
        command = ["switch", "--relocate",
                   from_info['Repository Root'],
                   to_info['Repository Root'],
                   self.relpath]
        RunSVN(command, self._root_dir)
        from_info['URL'] = from_info['URL'].replace(
            from_info['Repository Root'],
            to_info['Repository Root'])
      else:
        if CaptureSVNStatus(checkout_path):
          raise gclient_utils.Error("Can't switch the checkout to %s; UUID "
                                    "don't match and there is local changes "
                                    "in %s. Delete the directory and "
                                    "try again." % (url, checkout_path))
        # Ok delete it.
        print("\n_____ switching %s to a new checkout" % self.relpath)
        gclient_utils.RemoveDirectory(checkout_path)
        # We need to checkout.
        command = ['checkout', url, checkout_path]
        if revision:
          command.extend(['--revision', str(revision)])
        RunSVNAndGetFileList(command, self._root_dir, file_list)
        return


    # If the provided url has a revision number that matches the revision
    # number of the existing directory, then we don't need to bother updating.
    if not options.force and from_info['Revision'] == revision:
      if options.verbose or not forced_revision:
        print("\n_____ %s%s" % (self.relpath, rev_str))
      return

    command = ["update", checkout_path]
    if revision:
      command.extend(['--revision', str(revision)])
    RunSVNAndGetFileList(command, self._root_dir, file_list)

  def revert(self, options, args, file_list):
    """Reverts local modifications. Subversion specific.

    All reverted files will be appended to file_list, even if Subversion
    doesn't know about them.
    """
    path = os.path.join(self._root_dir, self.relpath)
    if not os.path.isdir(path):
      # svn revert won't work if the directory doesn't exist. It needs to
      # checkout instead.
      print("\n_____ %s is missing, synching instead" % self.relpath)
      # Don't reuse the args.
      return self.update(options, [], file_list)

    files = CaptureSVNStatus(path)
    # Batch the command.
    files_to_revert = []
    for file in files:
      file_path = os.path.join(path, file[1])
      print(file_path)
      # Unversioned file or unexpected unversioned file.
      if file[0][0] in ('?', '~'):
        # Remove extraneous file. Also remove unexpected unversioned
        # directories. svn won't touch them but we want to delete these.
        file_list.append(file_path)
        try:
          os.remove(file_path)
        except EnvironmentError:
          gclient_utils.RemoveDirectory(file_path)

      if file[0][0] != '?':
        # For any other status, svn revert will work.
        file_list.append(file_path)
        files_to_revert.append(file[1])

    # Revert them all at once.
    if files_to_revert:
      accumulated_paths = []
      accumulated_length = 0
      command = ['revert']
      for p in files_to_revert:
        # Some shell have issues with command lines too long.
        if accumulated_length and accumulated_length + len(p) > 3072:
          RunSVN(command + accumulated_paths,
                 os.path.join(self._root_dir, self.relpath))
          accumulated_paths = [p]
          accumulated_length = len(p)
        else:
          accumulated_paths.append(p)
          accumulated_length += len(p)
      if accumulated_paths:
        RunSVN(command + accumulated_paths,
               os.path.join(self._root_dir, self.relpath))

  def status(self, options, args, file_list):
    """Display status information."""
    path = os.path.join(self._root_dir, self.relpath)
    command = ['status']
    command.extend(args)
    if not os.path.isdir(path):
      # svn status won't work if the directory doesn't exist.
      print("\n________ couldn't run \'%s\' in \'%s\':\nThe directory "
            "does not exist."
            % (' '.join(command), path))
      # There's no file list to retrieve.
    else:
      RunSVNAndGetFileList(command, path, file_list)

  def pack(self, options, args, file_list):
    """Generates a patch file which can be applied to the root of the
    repository."""
    path = os.path.join(self._root_dir, self.relpath)
    command = ['diff']
    command.extend(args)
    # Simple class which tracks which file is being diffed and
    # replaces instances of its file name in the original and
    # working copy lines of the svn diff output.
    class DiffFilterer(object):
      index_string = "Index: "
      original_prefix = "--- "
      working_prefix = "+++ "

      def __init__(self, relpath):
        # Note that we always use '/' as the path separator to be
        # consistent with svn's cygwin-style output on Windows
        self._relpath = relpath.replace("\\", "/")
        self._current_file = ""
        self._replacement_file = ""

      def SetCurrentFile(self, file):
        self._current_file = file
        # Note that we always use '/' as the path separator to be
        # consistent with svn's cygwin-style output on Windows
        self._replacement_file = self._relpath + '/' + file

      def ReplaceAndPrint(self, line):
        print(line.replace(self._current_file, self._replacement_file))

      def Filter(self, line):
        if (line.startswith(self.index_string)):
          self.SetCurrentFile(line[len(self.index_string):])
          self.ReplaceAndPrint(line)
        else:
          if (line.startswith(self.original_prefix) or
              line.startswith(self.working_prefix)):
            self.ReplaceAndPrint(line)
          else:
            print line

    filterer = DiffFilterer(self.relpath)
    RunSVNAndFilterOutput(command, path, False, False, filterer.Filter)


# -----------------------------------------------------------------------------
# SVN utils:


def RunSVN(args, in_directory):
  """Runs svn, sending output to stdout.

  Args:
    args: A sequence of command line parameters to be passed to svn.
    in_directory: The directory where svn is to be run.

  Raises:
    Error: An error occurred while running the svn command.
  """
  c = [SVN_COMMAND]
  c.extend(args)

  gclient_utils.SubprocessCall(c, in_directory)


def CaptureSVN(args, in_directory=None, print_error=True):
  """Runs svn, capturing output sent to stdout as a string.

  Args:
    args: A sequence of command line parameters to be passed to svn.
    in_directory: The directory where svn is to be run.

  Returns:
    The output sent to stdout as a string.
  """
  c = [SVN_COMMAND]
  c.extend(args)

  # *Sigh*:  Windows needs shell=True, or else it won't search %PATH% for
  # the svn.exe executable, but shell=True makes subprocess on Linux fail
  # when it's called with a list because it only tries to execute the
  # first string ("svn").
  stderr = None
  if not print_error:
    stderr = subprocess.PIPE
  return subprocess.Popen(c,
                          cwd=in_directory,
                          shell=(sys.platform == 'win32'),
                          stdout=subprocess.PIPE,
                          stderr=stderr).communicate()[0]


def RunSVNAndGetFileList(args, in_directory, file_list):
  """Runs svn checkout, update, or status, output to stdout.

  The first item in args must be either "checkout", "update", or "status".

  svn's stdout is parsed to collect a list of files checked out or updated.
  These files are appended to file_list.  svn's stdout is also printed to
  sys.stdout as in RunSVN.

  Args:
    args: A sequence of command line parameters to be passed to svn.
    in_directory: The directory where svn is to be run.

  Raises:
    Error: An error occurred while running the svn command.
  """
  command = [SVN_COMMAND]
  command.extend(args)

  # svn update and svn checkout use the same pattern: the first three columns
  # are for file status, property status, and lock status.  This is followed
  # by two spaces, and then the path to the file.
  update_pattern = '^...  (.*)$'

  # The first three columns of svn status are the same as for svn update and
  # svn checkout.  The next three columns indicate addition-with-history,
  # switch, and remote lock status.  This is followed by one space, and then
  # the path to the file.
  status_pattern = '^...... (.*)$'

  # args[0] must be a supported command.  This will blow up if it's something
  # else, which is good.  Note that the patterns are only effective when
  # these commands are used in their ordinary forms, the patterns are invalid
  # for "svn status --show-updates", for example.
  pattern = {
        'checkout': update_pattern,
        'status':   status_pattern,
        'update':   update_pattern,
      }[args[0]]

  compiled_pattern = re.compile(pattern)

  def CaptureMatchingLines(line):
    match = compiled_pattern.search(line)
    if match:
      file_list.append(match.group(1))

  RunSVNAndFilterOutput(args,
                        in_directory,
                        True,
                        True,
                        CaptureMatchingLines)

def RunSVNAndFilterOutput(args,
                          in_directory,
                          print_messages,
                          print_stdout,
                          filter):
  """Runs svn checkout, update, status, or diff, optionally outputting
  to stdout.

  The first item in args must be either "checkout", "update",
  "status", or "diff".

  svn's stdout is passed line-by-line to the given filter function. If
  print_stdout is true, it is also printed to sys.stdout as in RunSVN.

  Args:
    args: A sequence of command line parameters to be passed to svn.
    in_directory: The directory where svn is to be run.
    print_messages: Whether to print status messages to stdout about
      which Subversion commands are being run.
    print_stdout: Whether to forward Subversion's output to stdout.
    filter: A function taking one argument (a string) which will be
      passed each line (with the ending newline character removed) of
      Subversion's output for filtering.

  Raises:
    Error: An error occurred while running the svn command.
  """
  command = [SVN_COMMAND]
  command.extend(args)

  gclient_utils.SubprocessCallAndFilter(command,
                                        in_directory,
                                        print_messages,
                                        print_stdout,
                                        filter=filter)

def CaptureSVNInfo(relpath, in_directory=None, print_error=True):
  """Returns a dictionary from the svn info output for the given file.

  Args:
    relpath: The directory where the working copy resides relative to
      the directory given by in_directory.
    in_directory: The directory where svn is to be run.
  """
  output = CaptureSVN(["info", "--xml", relpath], in_directory, print_error)
  dom = gclient_utils.ParseXML(output)
  result = {}
  if dom:
    GetNamedNodeText = gclient_utils.GetNamedNodeText
    GetNodeNamedAttributeText = gclient_utils.GetNodeNamedAttributeText
    def C(item, f):
      if item is not None: return f(item)
    # /info/entry/
    #   url
    #   reposityory/(root|uuid)
    #   wc-info/(schedule|depth)
    #   commit/(author|date)
    # str() the results because they may be returned as Unicode, which
    # interferes with the higher layers matching up things in the deps
    # dictionary.
    # TODO(maruel): Fix at higher level instead (!)
    result['Repository Root'] = C(GetNamedNodeText(dom, 'root'), str)
    result['URL'] = C(GetNamedNodeText(dom, 'url'), str)
    result['UUID'] = C(GetNamedNodeText(dom, 'uuid'), str)
    result['Revision'] = C(GetNodeNamedAttributeText(dom, 'entry', 'revision'),
                           int)
    result['Node Kind'] = C(GetNodeNamedAttributeText(dom, 'entry', 'kind'),
                            str)
    result['Schedule'] = C(GetNamedNodeText(dom, 'schedule'), str)
    result['Path'] = C(GetNodeNamedAttributeText(dom, 'entry', 'path'), str)
    result['Copied From URL'] = C(GetNamedNodeText(dom, 'copy-from-url'), str)
    result['Copied From Rev'] = C(GetNamedNodeText(dom, 'copy-from-rev'), str)
  return result


def CaptureSVNHeadRevision(url):
  """Get the head revision of a SVN repository.

  Returns:
    Int head revision
  """
  info = CaptureSVN(["info", "--xml", url], os.getcwd())
  dom = xml.dom.minidom.parseString(info)
  return int(dom.getElementsByTagName('entry')[0].getAttribute('revision'))


def CaptureSVNStatus(files):
  """Returns the svn 1.5 svn status emulated output.

  @files can be a string (one file) or a list of files.

  Returns an array of (status, file) tuples."""
  command = ["status", "--xml"]
  if not files:
    pass
  elif isinstance(files, basestring):
    command.append(files)
  else:
    command.extend(files)

  status_letter = {
    None: ' ',
    '': ' ',
    'added': 'A',
    'conflicted': 'C',
    'deleted': 'D',
    'external': 'X',
    'ignored': 'I',
    'incomplete': '!',
    'merged': 'G',
    'missing': '!',
    'modified': 'M',
    'none': ' ',
    'normal': ' ',
    'obstructed': '~',
    'replaced': 'R',
    'unversioned': '?',
  }
  dom = gclient_utils.ParseXML(CaptureSVN(command))
  results = []
  if dom:
    # /status/target/entry/(wc-status|commit|author|date)
    for target in dom.getElementsByTagName('target'):
      base_path = target.getAttribute('path')
      for entry in target.getElementsByTagName('entry'):
        file = entry.getAttribute('path')
        wc_status = entry.getElementsByTagName('wc-status')
        assert len(wc_status) == 1
        # Emulate svn 1.5 status ouput...
        statuses = [' ' for i in range(7)]
        # Col 0
        xml_item_status = wc_status[0].getAttribute('item')
        if xml_item_status in status_letter:
          statuses[0] = status_letter[xml_item_status]
        else:
          raise Exception('Unknown item status "%s"; please implement me!' %
                          xml_item_status)
        # Col 1
        xml_props_status = wc_status[0].getAttribute('props')
        if xml_props_status == 'modified':
          statuses[1] = 'M'
        elif xml_props_status == 'conflicted':
          statuses[1] = 'C'
        elif (not xml_props_status or xml_props_status == 'none' or
              xml_props_status == 'normal'):
          pass
        else:
          raise Exception('Unknown props status "%s"; please implement me!' %
                          xml_props_status)
        # Col 2
        if wc_status[0].getAttribute('wc-locked') == 'true':
          statuses[2] = 'L'
        # Col 3
        if wc_status[0].getAttribute('copied') == 'true':
          statuses[3] = '+'
        item = (''.join(statuses), file)
        results.append(item)
  return results

#!/usr/bin/python
#
# Copyright 2008 Google Inc.  All Rights Reserved.
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

"""A wrapper script to manage a set of client modules in different SCM.

This script is intended to be used to help basic management of client
program sources residing in one or more Subversion modules, along with
other modules it depends on, also in Subversion, but possibly on
multiple respositories, making a wrapper system apparently necessary.

Files
  .gclient      : Current client configuration, written by 'config' command.
                  Format is a Python script defining 'solutions', a list whose
                  entries each are maps binding the strings "name" and "url"
                  to strings specifying the name and location of the client
                  module, as well as "custom_deps" to a map similar to the DEPS
                  file below.
  .gclient_entries : A cache constructed by 'update' command.  Format is a
                  Python script defining 'entries', a list of the names
                  of all modules in the client
  <module>/DEPS : Python script defining var 'deps' as a map from each requisite
                  submodule name to a URL where it can be found (via one SCM)

Hooks
  .gclient and DEPS files may optionally contain a list named "hooks" to
  allow custom actions to be performed based on files that have changed in the
  working copy as a result of a "sync"/"update" or "revert" operation.  This
  could be prevented by using --nohooks (hooks run by default). Hooks can also
  be forced to run with the "runhooks" operation.  If "sync" is run with
  --force, all known hooks will run regardless of the state of the working
  copy.

  Each item in a "hooks" list is a dict, containing these two keys:
    "pattern"  The associated value is a string containing a regular
               expression.  When a file whose pathname matches the expression
               is checked out, updated, or reverted, the hook's "action" will
               run.
    "action"   A list describing a command to run along with its arguments, if
               any.  An action command will run at most one time per gclient
               invocation, regardless of how many files matched the pattern.
               The action is executed in the same directory as the .gclient
               file.  If the first item in the list is the string "python",
               the current Python interpreter (sys.executable) will be used
               to run the command. If the list contains string "$matching_files"
               it will be removed from the list and the list will be extended
               by the list of matching files.

  Example:
    hooks = [
      { "pattern": "\\.(gif|jpe?g|pr0n|png)$",
        "action":  ["python", "image_indexer.py", "--all"]},
    ]
"""

__author__ = "darinf@gmail.com (Darin Fisher)"
__version__ = "0.3.3"

import errno
import optparse
import os
import re
import stat
import subprocess
import sys
import time
import urlparse
import xml.dom.minidom
import urllib


SVN_COMMAND = "svn"


# default help text
DEFAULT_USAGE_TEXT = (
"""usage: %prog <subcommand> [options] [--] [svn options/args...]
a wrapper for managing a set of client modules in svn.
Version """ + __version__ + """

subcommands:
   cleanup
   config
   diff
   export
   pack
   revert
   status
   sync
   update
   runhooks
   revinfo

Options and extra arguments can be passed to invoked svn commands by
appending them to the command line.  Note that if the first such
appended option starts with a dash (-) then the options must be
preceded by -- to distinguish them from gclient options.

For additional help on a subcommand or examples of usage, try
   %prog help <subcommand>
   %prog help files
""")

GENERIC_UPDATE_USAGE_TEXT = (
    """Perform a checkout/update of the modules specified by the gclient
configuration; see 'help config'.  Unless --revision is specified,
then the latest revision of the root solutions is checked out, with
dependent submodule versions updated according to DEPS files.
If --revision is specified, then the given revision is used in place
of the latest, either for a single solution or for all solutions.
Unless the --force option is provided, solutions and modules whose
local revision matches the one to update (i.e., they have not changed
in the repository) are *not* modified. Unless --nohooks is provided,
the hooks are run.
This a synonym for 'gclient %(alias)s'

usage: gclient %(cmd)s [options] [--] [svn update options/args]

Valid options:
  --force             : force update even for unchanged modules
  --nohooks           : don't run the hooks after the update is complete
  --revision REV      : update/checkout all solutions with specified revision
  --revision SOLUTION@REV : update given solution to specified revision
  --deps PLATFORM(S)  : sync deps for the given platform(s), or 'all'
  --verbose           : output additional diagnostics
  --head              : update to latest revision, instead of last good revision

Examples:
  gclient %(cmd)s
      update files from SVN according to current configuration,
      *for modules which have changed since last update or sync*
  gclient %(cmd)s --force
      update files from SVN according to current configuration, for
      all modules (useful for recovering files deleted from local copy)
""")

COMMAND_USAGE_TEXT = {
    "cleanup":
    """Clean up all working copies, using 'svn cleanup' for each module.
Additional options and args may be passed to 'svn cleanup'.

usage: cleanup [options] [--] [svn cleanup args/options]

Valid options:
  --verbose           : output additional diagnostics
""",
    "config": """Create a .gclient file in the current directory; this
specifies the configuration for further commands.  After update/sync,
top-level DEPS files in each module are read to determine dependent
modules to operate on as well.  If optional [url] parameter is
provided, then configuration is read from a specified Subversion server
URL.  Otherwise, a --spec option must be provided.

usage: config [option | url] [safesync url]

Valid options:
  --spec=GCLIENT_SPEC   : contents of .gclient are read from string parameter.
                          *Note that due to Cygwin/Python brokenness, it
                          probably can't contain any newlines.*

Examples:
  gclient config https://gclient.googlecode.com/svn/trunk/gclient
      configure a new client to check out gclient.py tool sources
  gclient config --spec='solutions=[{"name":"gclient","""
    '"url":"https://gclient.googlecode.com/svn/trunk/gclient",'
    '"custom_deps":{}}]',
    "diff": """Display the differences between two revisions of modules.
(Does 'svn diff' for each checked out module and dependences.)
Additional args and options to 'svn diff' can be passed after
gclient options.

usage: diff [options] [--] [svn args/options]

Valid options:
  --verbose            : output additional diagnostics

Examples:
  gclient diff
      simple 'svn diff' for configured client and dependences
  gclient diff -- -x -b
      use 'svn diff -x -b' to suppress whitespace-only differences
  gclient diff -- -r HEAD -x -b
      diff versus the latest version of each module
""",
    "export":
    """Wrapper for svn export for all managed directories
""",
    "pack":

    """Generate a patch which can be applied at the root of the tree.
Internally, runs 'svn diff' on each checked out module and
dependencies, and performs minimal postprocessing of the output. The
resulting patch is printed to stdout and can be applied to a freshly
checked out tree via 'patch -p0 < patchfile'. Additional args and
options to 'svn diff' can be passed after gclient options.

usage: pack [options] [--] [svn args/options]

Valid options:
  --verbose            : output additional diagnostics

Examples:
  gclient pack > patch.txt
      generate simple patch for configured client and dependences
  gclient pack -- -x -b > patch.txt
      generate patch using 'svn diff -x -b' to suppress
      whitespace-only differences
  gclient pack -- -r HEAD -x -b > patch.txt
      generate patch, diffing each file versus the latest version of
      each module
""",
    "revert":
    """Revert every file in every managed directory in the client view.

usage: revert
""",
    "status":
    """Show the status of client and dependent modules, using 'svn diff'
for each module.  Additional options and args may be passed to 'svn diff'.

usage: status [options] [--] [svn diff args/options]

Valid options:
  --verbose           : output additional diagnostics
  --nohooks           : don't run the hooks after the update is complete
""",
    "sync": GENERIC_UPDATE_USAGE_TEXT % {"cmd": "sync", "alias": "update"},
    "update": GENERIC_UPDATE_USAGE_TEXT % {"cmd": "update", "alias": "sync"},
    "help": """Describe the usage of this program or its subcommands.

usage: help [options] [subcommand]

Valid options:
  --verbose           : output additional diagnostics
""",
    "runhooks":
    """Runs hooks for files that have been modified in the local working copy,
according to 'svn status'. Implies --force.

usage: runhooks [options]

Valid options:
  --verbose           : output additional diagnostics
""",
    "revinfo":
    """Outputs source path, server URL and revision information for every
dependency in all solutions (no local checkout required).

usage: revinfo [options]
""",
}

DEFAULT_CLIENT_FILE_TEXT = ("""\
# An element of this array (a "solution") describes a repository directory
# that will be checked out into your working copy.  Each solution may
# optionally define additional dependencies (via its DEPS file) to be
# checked out alongside the solution's directory.  A solution may also
# specify custom dependencies (via the "custom_deps" property) that
# override or augment the dependencies specified by the DEPS file.
# If a "safesync_url" is specified, it is assumed to reference the location of
# a text file which contains nothing but the last known good SCM revision to
# sync against. It is fetched if specified and used unless --head is passed

solutions = [
  { "name"        : "%(solution_name)s",
    "url"         : "%(solution_url)s",
    "custom_deps" : {
      # To use the trunk of a component instead of what's in DEPS:
      #"component": "https://svnserver/component/trunk/",
      # To exclude a component from your working copy:
      #"data/really_large_component": None,
    },
    "safesync_url": "%(safesync_url)s"
  },
]
""")


## Generic utils

def ParseXML(output):
  try:
    return xml.dom.minidom.parseString(output)
  except xml.parsers.expat.ExpatError:
    return None


def GetNamedNodeText(node, node_name):
  child_nodes = node.getElementsByTagName(node_name)
  if not child_nodes:
    return None
  assert len(child_nodes) == 1 and child_nodes[0].childNodes.length == 1
  return child_nodes[0].firstChild.nodeValue


def GetNodeNamedAttributeText(node, node_name, attribute_name):
  child_nodes = node.getElementsByTagName(node_name)
  if not child_nodes:
    return None
  assert len(child_nodes) == 1
  return child_nodes[0].getAttribute(attribute_name)


class Error(Exception):
  """gclient exception class."""
  pass

class PrintableObject(object):
  def __str__(self):
    output = ''
    for i in dir(self):
      if i.startswith('__'):
        continue
      output += '%s = %s\n' % (i, str(getattr(self, i, '')))
    return output


def FileRead(filename):
  content = None
  f = open(filename, "rU")
  try:
    content = f.read()
  finally:
    f.close()
  return content


def FileWrite(filename, content):
  f = open(filename, "w")
  try:
    f.write(content)
  finally:
    f.close()


def RemoveDirectory(*path):
  """Recursively removes a directory, even if it's marked read-only.

  Remove the directory located at *path, if it exists.

  shutil.rmtree() doesn't work on Windows if any of the files or directories
  are read-only, which svn repositories and some .svn files are.  We need to
  be able to force the files to be writable (i.e., deletable) as we traverse
  the tree.

  Even with all this, Windows still sometimes fails to delete a file, citing
  a permission error (maybe something to do with antivirus scans or disk
  indexing).  The best suggestion any of the user forums had was to wait a
  bit and try again, so we do that too.  It's hand-waving, but sometimes it
  works. :/

  On POSIX systems, things are a little bit simpler.  The modes of the files
  to be deleted doesn't matter, only the modes of the directories containing
  them are significant.  As the directory tree is traversed, each directory
  has its mode set appropriately before descending into it.  This should
  result in the entire tree being removed, with the possible exception of
  *path itself, because nothing attempts to change the mode of its parent.
  Doing so would be hazardous, as it's not a directory slated for removal.
  In the ordinary case, this is not a problem: for our purposes, the user
  will never lack write permission on *path's parent.
  """
  file_path = os.path.join(*path)
  if not os.path.exists(file_path):
    return

  if os.path.islink(file_path) or not os.path.isdir(file_path):
    raise Error("RemoveDirectory asked to remove non-directory %s" % file_path)

  has_win32api = False
  if sys.platform == 'win32':
    has_win32api = True
    # Some people don't have the APIs installed. In that case we'll do without.
    try:
      win32api = __import__('win32api')
      win32con = __import__('win32con')
    except ImportError:
      has_win32api = False
  else:
    # On POSIX systems, we need the x-bit set on the directory to access it,
    # the r-bit to see its contents, and the w-bit to remove files from it.
    # The actual modes of the files within the directory is irrelevant.
    os.chmod(file_path, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)
  for fn in os.listdir(file_path):
    fullpath = os.path.join(file_path, fn)

    # If fullpath is a symbolic link that points to a directory, isdir will
    # be True, but we don't want to descend into that as a directory, we just
    # want to remove the link.  Check islink and treat links as ordinary files
    # would be treated regardless of what they reference.
    if os.path.islink(fullpath) or not os.path.isdir(fullpath):
      if sys.platform == 'win32':
        os.chmod(fullpath, stat.S_IWRITE)
        if has_win32api:
          win32api.SetFileAttributes(fullpath, win32con.FILE_ATTRIBUTE_NORMAL)
      try:
        os.remove(fullpath)
      except OSError, e:
        if e.errno != errno.EACCES or sys.platform != 'win32':
          raise
        print 'Failed to delete %s: trying again' % fullpath
        time.sleep(0.1)
        os.remove(fullpath)
    else:
      RemoveDirectory(fullpath)

  if sys.platform == 'win32':
    os.chmod(file_path, stat.S_IWRITE)
    if has_win32api:
      win32api.SetFileAttributes(file_path, win32con.FILE_ATTRIBUTE_NORMAL)
  try:
    os.rmdir(file_path)
  except OSError, e:
    if e.errno != errno.EACCES or sys.platform != 'win32':
      raise
    print 'Failed to remove %s: trying again' % file_path
    time.sleep(0.1)
    os.rmdir(file_path)


def SubprocessCall(command, in_directory, fail_status=None):
  """Runs command, a list, in directory in_directory.

  This function wraps SubprocessCallAndFilter, but does not perform the
  filtering functions.  See that function for a more complete usage
  description.
  """
  # Call subprocess and capture nothing:
  SubprocessCallAndFilter(command, in_directory, True, True, fail_status)


def SubprocessCallAndFilter(command,
                            in_directory,
                            print_messages,
                            print_stdout,
                            fail_status=None, filter=None):
  """Runs command, a list, in directory in_directory.

  If print_messages is true, a message indicating what is being done
  is printed to stdout. If print_stdout is true, the command's stdout
  is also forwarded to stdout.

  If a filter function is specified, it is expected to take a single
  string argument, and it will be called with each line of the
  subprocess's output. Each line has had the trailing newline character
  trimmed.

  If the command fails, as indicated by a nonzero exit status, gclient will
  exit with an exit status of fail_status.  If fail_status is None (the
  default), gclient will raise an Error exception.
  """

  if print_messages:
    print("\n________ running \'%s\' in \'%s\'"
          % (' '.join(command), in_directory))

  # *Sigh*:  Windows needs shell=True, or else it won't search %PATH% for the
  # executable, but shell=True makes subprocess on Linux fail when it's called
  # with a list because it only tries to execute the first item in the list.
  kid = subprocess.Popen(command, bufsize=0, cwd=in_directory,
      shell=(sys.platform == 'win32'), stdout=subprocess.PIPE)

  # Also, we need to forward stdout to prevent weird re-ordering of output.
  # This has to be done on a per byte basis to make sure it is not buffered:
  # normally buffering is done for each line, but if svn requests input, no
  # end-of-line character is output after the prompt and it would not show up.
  in_byte = kid.stdout.read(1)
  in_line = ""
  while in_byte:
    if in_byte != "\r":
      if print_stdout:
        sys.stdout.write(in_byte)
      if in_byte != "\n":
        in_line += in_byte
    if in_byte == "\n" and filter:
      filter(in_line)
      in_line = ""
    in_byte = kid.stdout.read(1)
  rv = kid.wait()

  if rv:
    msg = "failed to run command: %s" % " ".join(command)

    if fail_status != None:
      print >>sys.stderr, msg
      sys.exit(fail_status)

    raise Error(msg)


def IsUsingGit(root, paths):
  """Returns True if we're using git to manage any of our checkouts.
  |entries| is a list of paths to check."""
  for path in paths:
    if os.path.exists(os.path.join(root, path, '.git')):
      return True
  return False

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

  SubprocessCall(c, in_directory)


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

  SubprocessCallAndFilter(command,
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
  dom = ParseXML(output)
  result = {}
  if dom:
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
  dom = ParseXML(CaptureSVN(command))
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

    if file_list == None:
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
      raise Error('Unknown command %s' % command)

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
      raise Error("Unsupported argument(s): %s" % ",".join(args))

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
      raise Error("Can't update/checkout %r if an unversioned directory is "
                  "present. Delete the directory and try again." %
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
          raise Error("Can't switch the checkout to %s; UUID don't match and "
                      "there is local changes in %s. Delete the directory and "
                      "try again." % (url, checkout_path))
        # Ok delete it.
        print("\n_____ switching %s to a new checkout" % self.relpath)
        RemoveDirectory(checkout_path)
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
          RemoveDirectory(file_path)

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
          accumulated_paths = []
          accumulated_length = 0
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

## GClient implementation.


class GClient(object):
  """Object that represent a gclient checkout."""

  supported_commands = [
    'cleanup', 'diff', 'export', 'pack', 'revert', 'status', 'update',
    'runhooks'
  ]

  def __init__(self, root_dir, options):
    self._root_dir = root_dir
    self._options = options
    self._config_content = None
    self._config_dict = {}
    self._deps_hooks = []

  def SetConfig(self, content):
    self._config_dict = {}
    self._config_content = content
    try:
      exec(content, self._config_dict)
    except SyntaxError, e:
      try:
        # Try to construct a human readable error message
        error_message = [
            'There is a syntax error in your configuration file.',
            'Line #%s, character %s:' % (e.lineno, e.offset),
            '"%s"' % re.sub(r'[\r\n]*$', '', e.text) ]
      except:
        # Something went wrong, re-raise the original exception
        raise e
      else:
        # Raise a new exception with the human readable message:
        raise Error('\n'.join(error_message))

  def SaveConfig(self):
    FileWrite(os.path.join(self._root_dir, self._options.config_filename),
              self._config_content)

  def _LoadConfig(self):
    client_source = FileRead(os.path.join(self._root_dir,
                                          self._options.config_filename))
    self.SetConfig(client_source)

  def ConfigContent(self):
    return self._config_content

  def GetVar(self, key, default=None):
    return self._config_dict.get(key, default)

  @staticmethod
  def LoadCurrentConfig(options, from_dir=None):
    """Searches for and loads a .gclient file relative to the current working
    dir.

    Returns:
      A dict representing the contents of the .gclient file or an empty dict if
      the .gclient file doesn't exist.
    """
    if not from_dir:
      from_dir = os.curdir
    path = os.path.realpath(from_dir)
    while not os.path.exists(os.path.join(path, options.config_filename)):
      next = os.path.split(path)
      if not next[1]:
        return None
      path = next[0]
    client = GClient(path, options)
    client._LoadConfig()
    return client

  def SetDefaultConfig(self, solution_name, solution_url, safesync_url):
    self.SetConfig(DEFAULT_CLIENT_FILE_TEXT % {
      'solution_name': solution_name,
      'solution_url': solution_url,
      'safesync_url' : safesync_url,
    })

  def _SaveEntries(self, entries):
    """Creates a .gclient_entries file to record the list of unique checkouts.

    The .gclient_entries file lives in the same directory as .gclient.

    Args:
      entries: A sequence of solution names.
    """
    text = "entries = [\n"
    for entry in entries:
      text += "  \"%s\",\n" % entry
    text += "]\n"
    FileWrite(os.path.join(self._root_dir, self._options.entries_filename),
              text)

  def _ReadEntries(self):
    """Read the .gclient_entries file for the given client.

    Args:
      client: The client for which the entries file should be read.

    Returns:
      A sequence of solution names, which will be empty if there is the
      entries file hasn't been created yet.
    """
    scope = {}
    filename = os.path.join(self._root_dir, self._options.entries_filename)
    if not os.path.exists(filename):
      return []
    exec(FileRead(filename), scope)
    return scope["entries"]

  class FromImpl:
    """Used to implement the From syntax."""

    def __init__(self, module_name):
      self.module_name = module_name

    def __str__(self):
      return 'From("%s")' % self.module_name

  class _VarImpl:
    def __init__(self, custom_vars, local_scope):
      self._custom_vars = custom_vars
      self._local_scope = local_scope

    def Lookup(self, var_name):
      """Implements the Var syntax."""
      if var_name in self._custom_vars:
        return self._custom_vars[var_name]
      elif var_name in self._local_scope.get("vars", {}):
        return self._local_scope["vars"][var_name]
      raise Error("Var is not defined: %s" % var_name)

  def _ParseSolutionDeps(self, solution_name, solution_deps_content,
                         custom_vars):
    """Parses the DEPS file for the specified solution.

    Args:
      solution_name: The name of the solution to query.
      solution_deps_content: Content of the DEPS file for the solution
      custom_vars: A dict of vars to override any vars defined in the DEPS file.

    Returns:
      A dict mapping module names (as relative paths) to URLs or an empty
      dict if the solution does not have a DEPS file.
    """
    # Skip empty
    if not solution_deps_content:
      return {}
    # Eval the content
    local_scope = {}
    var = self._VarImpl(custom_vars, local_scope)
    global_scope = {"From": self.FromImpl, "Var": var.Lookup, "deps_os": {}}
    exec(solution_deps_content, global_scope, local_scope)
    deps = local_scope.get("deps", {})

    # load os specific dependencies if defined.  these dependencies may
    # override or extend the values defined by the 'deps' member.
    if "deps_os" in local_scope:
      deps_os_choices = {
          "win32": "win",
          "win": "win",
          "cygwin": "win",
          "darwin": "mac",
          "mac": "mac",
          "unix": "unix",
          "linux": "unix",
          "linux2": "unix",
         }

      if self._options.deps_os is not None:
        deps_to_include = self._options.deps_os.split(",")
        if "all" in deps_to_include:
          deps_to_include = deps_os_choices.values()
      else:
        deps_to_include = [deps_os_choices.get(self._options.platform, "unix")]

      deps_to_include = set(deps_to_include)
      for deps_os_key in deps_to_include:
        os_deps = local_scope["deps_os"].get(deps_os_key, {})
        if len(deps_to_include) > 1:
          # Ignore any overrides when including deps for more than one
          # platform, so we collect the broadest set of dependencies available.
          # We may end up with the wrong revision of something for our
          # platform, but this is the best we can do.
          deps.update([x for x in os_deps.items() if not x[0] in deps])
        else:
          deps.update(os_deps)

    if 'hooks' in local_scope:
      self._deps_hooks.extend(local_scope['hooks'])

    # If use_relative_paths is set in the DEPS file, regenerate
    # the dictionary using paths relative to the directory containing
    # the DEPS file.
    if local_scope.get('use_relative_paths'):
      rel_deps = {}
      for d, url in deps.items():
        # normpath is required to allow DEPS to use .. in their
        # dependency local path.
        rel_deps[os.path.normpath(os.path.join(solution_name, d))] = url
      return rel_deps
    else:
      return deps

  def _ParseAllDeps(self, solution_urls, solution_deps_content):
    """Parse the complete list of dependencies for the client.

    Args:
      solution_urls: A dict mapping module names (as relative paths) to URLs
        corresponding to the solutions specified by the client.  This parameter
        is passed as an optimization.
      solution_deps_content: A dict mapping module names to the content
        of their DEPS files

    Returns:
      A dict mapping module names (as relative paths) to URLs corresponding
      to the entire set of dependencies to checkout for the given client.

    Raises:
      Error: If a dependency conflicts with another dependency or of a solution.
    """
    deps = {}
    for solution in self.GetVar("solutions"):
      custom_vars = solution.get("custom_vars", {})
      solution_deps = self._ParseSolutionDeps(
                              solution["name"],
                              solution_deps_content[solution["name"]],
                              custom_vars)

      # If a line is in custom_deps, but not in the solution, we want to append
      # this line to the solution.
      if "custom_deps" in solution:
        for d in solution["custom_deps"]:
          if d not in solution_deps:
            solution_deps[d] = solution["custom_deps"][d]

      for d in solution_deps:
        if "custom_deps" in solution and d in solution["custom_deps"]:
          # Dependency is overriden.
          url = solution["custom_deps"][d]
          if url is None:
            continue
        else:
          url = solution_deps[d]
          # if we have a From reference dependent on another solution, then
          # just skip the From reference. When we pull deps for the solution,
          # we will take care of this dependency.
          #
          # If multiple solutions all have the same From reference, then we
          # should only add one to our list of dependencies.
          if type(url) != str:
            if url.module_name in solution_urls:
              # Already parsed.
              continue
            if d in deps and type(deps[d]) != str:
              if url.module_name == deps[d].module_name:
                continue
          else:
            parsed_url = urlparse.urlparse(url)
            scheme = parsed_url[0]
            if not scheme:
              # A relative url. Fetch the real base.
              path = parsed_url[2]
              if path[0] != "/":
                raise Error(
                    "relative DEPS entry \"%s\" must begin with a slash" % d)
              # Create a scm just to query the full url.
              scm = SCMWrapper(solution["url"], self._root_dir, None)
              url = scm.FullUrlForRelativeUrl(url)
        if d in deps and deps[d] != url:
          raise Error(
              "Solutions have conflicting versions of dependency \"%s\"" % d)
        if d in solution_urls and solution_urls[d] != url:
          raise Error(
              "Dependency \"%s\" conflicts with specified solution" % d)
        # Grab the dependency.
        deps[d] = url
    return deps

  def _RunHookAction(self, hook_dict, matching_file_list):
    """Runs the action from a single hook.
    """
    command = hook_dict['action'][:]
    if command[0] == 'python':
      # If the hook specified "python" as the first item, the action is a
      # Python script.  Run it by starting a new copy of the same
      # interpreter.
      command[0] = sys.executable

    if '$matching_files' in command:
      splice_index = command.index('$matching_files')
      command[splice_index:splice_index + 1] = matching_file_list

    # Use a discrete exit status code of 2 to indicate that a hook action
    # failed.  Users of this script may wish to treat hook action failures
    # differently from VC failures.
    SubprocessCall(command, self._root_dir, fail_status=2)

  def _RunHooks(self, command, file_list, is_using_git):
    """Evaluates all hooks, running actions as needed.
    """
    # Hooks only run for these command types.
    if not command in ('update', 'revert', 'runhooks'):
      return

    # Hooks only run when --nohooks is not specified
    if self._options.nohooks:
      return

    # Get any hooks from the .gclient file.
    hooks = self.GetVar("hooks", [])
    # Add any hooks found in DEPS files.
    hooks.extend(self._deps_hooks)

    # If "--force" was specified, run all hooks regardless of what files have
    # changed.  If the user is using git, then we don't know what files have
    # changed so we always run all hooks.
    if self._options.force or is_using_git:
      for hook_dict in hooks:
        self._RunHookAction(hook_dict, [])
      return

    # Run hooks on the basis of whether the files from the gclient operation
    # match each hook's pattern.
    for hook_dict in hooks:
      pattern = re.compile(hook_dict['pattern'])
      matching_file_list = [file for file in file_list if pattern.search(file)]
      if matching_file_list:
        self._RunHookAction(hook_dict, matching_file_list)

  def RunOnDeps(self, command, args):
    """Runs a command on each dependency in a client and its dependencies.

    The module's dependencies are specified in its top-level DEPS files.

    Args:
      command: The command to use (e.g., 'status' or 'diff')
      args: list of str - extra arguments to add to the command line.

    Raises:
      Error: If the client has conflicting entries.
    """
    if not command in self.supported_commands:
      raise Error("'%s' is an unsupported command" % command)

    # Check for revision overrides.
    revision_overrides = {}
    for revision in self._options.revisions:
      if revision.find("@") == -1:
        raise Error(
            "Specify the full dependency when specifying a revision number.")
      revision_elem = revision.split("@")
      # Disallow conflicting revs
      if revision_overrides.has_key(revision_elem[0]) and \
         revision_overrides[revision_elem[0]] != revision_elem[1]:
        raise Error(
            "Conflicting revision numbers specified.")
      revision_overrides[revision_elem[0]] = revision_elem[1]

    solutions = self.GetVar("solutions")
    if not solutions:
      raise Error("No solution specified")

    # When running runhooks --force, there's no need to consult the SCM.
    # All known hooks are expected to run unconditionally regardless of working
    # copy state, so skip the SCM status check.
    run_scm = not (command == 'runhooks' and self._options.force)

    entries = {}
    entries_deps_content = {}
    file_list = []
    # Run on the base solutions first.
    for solution in solutions:
      name = solution["name"]
      deps_file = solution.get("deps_file", self._options.deps_file)
      if '/' in deps_file or '\\' in deps_file:
        raise Error("deps_file name must not be a path, just a filename.")
      if name in entries:
        raise Error("solution %s specified more than once" % name)
      url = solution["url"]
      entries[name] = url
      if run_scm:
        self._options.revision = revision_overrides.get(name)
        scm = SCMWrapper(url, self._root_dir, name)
        scm.RunCommand(command, self._options, args, file_list)
        file_list = [os.path.join(name, file.strip()) for file in file_list]
        self._options.revision = None
      try:
        deps_content = FileRead(os.path.join(self._root_dir, name,
                                             deps_file))
      except IOError, e:
        if e.errno != errno.ENOENT:
          raise
        deps_content = ""
      entries_deps_content[name] = deps_content

    # Process the dependencies next (sort alphanumerically to ensure that
    # containing directories get populated first and for readability)
    deps = self._ParseAllDeps(entries, entries_deps_content)
    deps_to_process = deps.keys()
    deps_to_process.sort()

    # First pass for direct dependencies.
    for d in deps_to_process:
      if type(deps[d]) == str:
        url = deps[d]
        entries[d] = url
        if run_scm:
          self._options.revision = revision_overrides.get(d)
          scm = SCMWrapper(url, self._root_dir, d)
          scm.RunCommand(command, self._options, args, file_list)
          self._options.revision = None

    # Second pass for inherited deps (via the From keyword)
    for d in deps_to_process:
      if type(deps[d]) != str:
        sub_deps = self._ParseSolutionDeps(
                           deps[d].module_name,
                           FileRead(os.path.join(self._root_dir,
                                                 deps[d].module_name,
                                                 self._options.deps_file)),
                           {})
        url = sub_deps[d]
        entries[d] = url
        if run_scm:
          self._options.revision = revision_overrides.get(d)
          scm = SCMWrapper(url, self._root_dir, d)
          scm.RunCommand(command, self._options, args, file_list)
          self._options.revision = None

    # Convert all absolute paths to relative.
    for i in range(len(file_list)):
      # TODO(phajdan.jr): We should know exactly when the paths are absolute.
      # It depends on the command being executed (like runhooks vs sync).
      if not os.path.isabs(file_list[i]):
        continue

      prefix = os.path.commonprefix([self._root_dir.lower(),
                                     file_list[i].lower()])
      file_list[i] = file_list[i][len(prefix):]

      # Strip any leading path separators.
      while file_list[i].startswith('\\') or file_list[i].startswith('/'):
        file_list[i] = file_list[i][1:]

    is_using_git = IsUsingGit(self._root_dir, entries.keys())
    self._RunHooks(command, file_list, is_using_git)

    if command == 'update':
      # Notify the user if there is an orphaned entry in their working copy.
      # Only delete the directory if there are no changes in it, and
      # delete_unversioned_trees is set to true.
      prev_entries = self._ReadEntries()
      for entry in prev_entries:
        # Fix path separator on Windows.
        entry_fixed = entry.replace('/', os.path.sep)
        e_dir = os.path.join(self._root_dir, entry_fixed)
        # Use entry and not entry_fixed there.
        if entry not in entries and os.path.exists(e_dir):
          if not self._options.delete_unversioned_trees or \
             CaptureSVNStatus(e_dir):
            # There are modified files in this entry. Keep warning until
            # removed.
            entries[entry] = None
            print(("\nWARNING: \"%s\" is no longer part of this client.  "
                   "It is recommended that you manually remove it.\n") %
                      entry_fixed)
          else:
            # Delete the entry
            print("\n________ deleting \'%s\' " +
                  "in \'%s\'") % (entry_fixed, self._root_dir)
            RemoveDirectory(e_dir)
      # record the current list of entries for next time
      self._SaveEntries(entries)

  def PrintRevInfo(self):
    """Output revision info mapping for the client and its dependencies. This
    allows the capture of a overall "revision" for the source tree that can
    be used to reproduce the same tree in the future. The actual output
    contains enough information (source paths, svn server urls and revisions)
    that it can be used either to generate external svn commands (without
    gclient) or as input to gclient's --rev option (with some massaging of
    the data).

    NOTE: Unlike RunOnDeps this does not require a local checkout and is run
    on the Pulse master. It MUST NOT execute hooks.

    Raises:
      Error: If the client has conflicting entries.
    """
    # Check for revision overrides.
    revision_overrides = {}
    for revision in self._options.revisions:
      if revision.find("@") < 0:
        raise Error(
            "Specify the full dependency when specifying a revision number.")
      revision_elem = revision.split("@")
      # Disallow conflicting revs
      if revision_overrides.has_key(revision_elem[0]) and \
         revision_overrides[revision_elem[0]] != revision_elem[1]:
        raise Error(
            "Conflicting revision numbers specified.")
      revision_overrides[revision_elem[0]] = revision_elem[1]

    solutions = self.GetVar("solutions")
    if not solutions:
      raise Error("No solution specified")

    entries = {}
    entries_deps_content = {}

    # Inner helper to generate base url and rev tuple (including honoring
    # |revision_overrides|)
    def GetURLAndRev(name, original_url):
      if original_url.find("@") < 0:
        if revision_overrides.has_key(name):
          return (original_url, int(revision_overrides[name]))
        else:
          # TODO(aharper): SVN/SCMWrapper cleanup (non-local commandset)
          return (original_url, CaptureSVNHeadRevision(original_url))
      else:
        url_components = original_url.split("@")
        if revision_overrides.has_key(name):
          return (url_components[0], int(revision_overrides[name]))
        else:
          return (url_components[0], int(url_components[1]))

    # Run on the base solutions first.
    for solution in solutions:
      name = solution["name"]
      if name in entries:
        raise Error("solution %s specified more than once" % name)
      (url, rev) = GetURLAndRev(name, solution["url"])
      entries[name] = "%s@%d" % (url, rev)
      # TODO(aharper): SVN/SCMWrapper cleanup (non-local commandset)
      entries_deps_content[name] = CaptureSVN(
                                     ["cat",
                                      "%s/%s@%d" % (url,
                                                    self._options.deps_file,
                                                    rev)],
                                     os.getcwd())

    # Process the dependencies next (sort alphanumerically to ensure that
    # containing directories get populated first and for readability)
    deps = self._ParseAllDeps(entries, entries_deps_content)
    deps_to_process = deps.keys()
    deps_to_process.sort()

    # First pass for direct dependencies.
    for d in deps_to_process:
      if type(deps[d]) == str:
        (url, rev) = GetURLAndRev(d, deps[d])
        entries[d] = "%s@%d" % (url, rev)

    # Second pass for inherited deps (via the From keyword)
    for d in deps_to_process:
      if type(deps[d]) != str:
        deps_parent_url = entries[deps[d].module_name]
        if deps_parent_url.find("@") < 0:
          raise Error("From %s missing revisioned url" % deps[d].module_name)
        deps_parent_url_components = deps_parent_url.split("@")
        # TODO(aharper): SVN/SCMWrapper cleanup (non-local commandset)
        deps_parent_content = CaptureSVN(
                                ["cat",
                                 "%s/%s@%s" % (deps_parent_url_components[0],
                                               self._options.deps_file,
                                               deps_parent_url_components[1])],
                                os.getcwd())
        sub_deps = self._ParseSolutionDeps(
                           deps[d].module_name,
                           FileRead(os.path.join(self._root_dir,
                                                 deps[d].module_name,
                                                 self._options.deps_file)),
                           {})
        (url, rev) = GetURLAndRev(d, sub_deps[d])
        entries[d] = "%s@%d" % (url, rev)
    print(";\n\n".join(["%s: %s" % (x, entries[x])
                        for x in sorted(entries.keys())]))


## gclient commands.


def DoCleanup(options, args):
  """Handle the cleanup subcommand.

  Raises:
    Error: if client isn't configured properly.
  """
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise Error("client not configured; see 'gclient config'")
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  options.verbose = True
  return client.RunOnDeps('cleanup', args)


def DoConfig(options, args):
  """Handle the config subcommand.

  Args:
    options: If options.spec set, a string providing contents of config file.
    args: The command line args.  If spec is not set,
          then args[0] is a string URL to get for config file.

  Raises:
    Error: on usage error
  """
  if len(args) < 1 and not options.spec:
    raise Error("required argument missing; see 'gclient help config'")
  if os.path.exists(options.config_filename):
    raise Error("%s file already exists in the current directory" %
                options.config_filename)
  client = GClient('.', options)
  if options.spec:
    client.SetConfig(options.spec)
  else:
    # TODO(darin): it would be nice to be able to specify an alternate relpath
    # for the given URL.
    base_url = args[0].rstrip('/')
    name = base_url.split("/")[-1]
    safesync_url = ""
    if len(args) > 1:
      safesync_url = args[1]
    client.SetDefaultConfig(name, base_url, safesync_url)
  client.SaveConfig()


def DoExport(options, args):
  """Handle the export subcommand.

  Raises:
    Error: on usage error
  """
  if len(args) != 1:
    raise Error("Need directory name")
  client = GClient.LoadCurrentConfig(options)

  if not client:
    raise Error("client not configured; see 'gclient config'")

  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  return client.RunOnDeps('export', args)

def DoHelp(options, args):
  """Handle the help subcommand giving help for another subcommand.

  Raises:
    Error: if the command is unknown.
  """
  if len(args) == 1 and args[0] in COMMAND_USAGE_TEXT:
    print(COMMAND_USAGE_TEXT[args[0]])
  else:
    raise Error("unknown subcommand '%s'; see 'gclient help'" % args[0])


def DoPack(options, args):
  """Handle the pack subcommand.

  Raises:
    Error: if client isn't configured properly.
  """
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise Error("client not configured; see 'gclient config'")
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  options.verbose = True
  return client.RunOnDeps('pack', args)


def DoStatus(options, args):
  """Handle the status subcommand.

  Raises:
    Error: if client isn't configured properly.
  """
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise Error("client not configured; see 'gclient config'")
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  options.verbose = True
  return client.RunOnDeps('status', args)


def DoUpdate(options, args):
  """Handle the update and sync subcommands.

  Raises:
    Error: if client isn't configured properly.
  """
  client = GClient.LoadCurrentConfig(options)

  if not client:
    raise Error("client not configured; see 'gclient config'")

  if not options.head:
    solutions = client.GetVar('solutions')
    if solutions:
      for s in solutions:
        if s.get('safesync_url', ''):
          # rip through revisions and make sure we're not over-riding
          # something that was explicitly passed
          has_key = False
          for r in options.revisions:
            if r.split('@')[0] == s['name']:
              has_key = True
              break

          if not has_key:
            handle = urllib.urlopen(s['safesync_url'])
            rev = handle.read().strip()
            handle.close()
            if len(rev):
              options.revisions.append(s['name']+'@'+rev)

  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  return client.RunOnDeps('update', args)


def DoDiff(options, args):
  """Handle the diff subcommand.

  Raises:
    Error: if client isn't configured properly.
  """
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise Error("client not configured; see 'gclient config'")
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  options.verbose = True
  return client.RunOnDeps('diff', args)


def DoRevert(options, args):
  """Handle the revert subcommand.

  Raises:
    Error: if client isn't configured properly.
  """
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise Error("client not configured; see 'gclient config'")
  return client.RunOnDeps('revert', args)


def DoRunHooks(options, args):
  """Handle the runhooks subcommand.

  Raises:
    Error: if client isn't configured properly.
  """
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise Error("client not configured; see 'gclient config'")
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  options.force = True
  return client.RunOnDeps('runhooks', args)


def DoRevInfo(options, args):
  """Handle the revinfo subcommand.

  Raises:
    Error: if client isn't configured properly.
  """
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise Error("client not configured; see 'gclient config'")
  client.PrintRevInfo()


gclient_command_map = {
  "cleanup": DoCleanup,
  "config": DoConfig,
  "diff": DoDiff,
  "export": DoExport,
  "help": DoHelp,
  "pack": DoPack,
  "status": DoStatus,
  "sync": DoUpdate,
  "update": DoUpdate,
  "revert": DoRevert,
  "runhooks": DoRunHooks,
  "revinfo" : DoRevInfo,
}


def DispatchCommand(command, options, args, command_map=None):
  """Dispatches the appropriate subcommand based on command line arguments."""
  if command_map is None:
    command_map = gclient_command_map

  if command in command_map:
    return command_map[command](options, args)
  else:
    raise Error("unknown subcommand '%s'; see 'gclient help'" % command)


def Main(argv):
  """Parse command line arguments and dispatch command."""

  option_parser = optparse.OptionParser(usage=DEFAULT_USAGE_TEXT,
                                        version=__version__)
  option_parser.disable_interspersed_args()
  option_parser.add_option("", "--force", action="store_true", default=False,
                           help=("(update/sync only) force update even "
                                 "for modules which haven't changed"))
  option_parser.add_option("", "--nohooks", action="store_true", default=False,
                           help=("(update/sync/revert only) prevent the hooks from "
                                 "running"))
  option_parser.add_option("", "--revision", action="append", dest="revisions",
                           metavar="REV", default=[],
                           help=("(update/sync only) sync to a specific "
                                 "revision, can be used multiple times for "
                                 "each solution, e.g. --revision=src@123, "
                                 "--revision=internal@32"))
  option_parser.add_option("", "--deps", default=None, dest="deps_os",
                           metavar="OS_LIST",
                           help=("(update/sync only) sync deps for the "
                                 "specified (comma-separated) platform(s); "
                                 "'all' will sync all platforms"))
  option_parser.add_option("", "--spec", default=None,
                           help=("(config only) create a gclient file "
                                 "containing the provided string"))
  option_parser.add_option("", "--verbose", action="store_true", default=False,
                           help="produce additional output for diagnostics")
  option_parser.add_option("", "--manually_grab_svn_rev", action="store_true",
                           default=False,
                           help="Skip svn up whenever possible by requesting "
                                "actual HEAD revision from the repository")
  option_parser.add_option("", "--head", action="store_true", default=False,
                           help=("skips any safesync_urls specified in "
                                 "configured solutions"))
  option_parser.add_option("", "--delete_unversioned_trees",
                           action="store_true", default=False,
                           help=("on update, delete any unexpected "
                                 "unversioned trees that are in the checkout"))

  if len(argv) < 2:
    # Users don't need to be told to use the 'help' command.
    option_parser.print_help()
    return 1
  # Add manual support for --version as first argument.
  if argv[1] == '--version':
    option_parser.print_version()
    return 0

  # Add manual support for --help as first argument.
  if argv[1] == '--help':
    argv[1] = 'help'

  command = argv[1]
  options, args = option_parser.parse_args(argv[2:])

  if len(argv) < 3 and command == "help":
    option_parser.print_help()
    return 0

  # Files used for configuration and state saving.
  options.config_filename = os.environ.get("GCLIENT_FILE", ".gclient")
  options.entries_filename = ".gclient_entries"
  options.deps_file = "DEPS"

  options.platform = sys.platform
  return DispatchCommand(command, options, args)


if "__main__" == __name__:
  try:
    result = Main(sys.argv)
  except Error, e:
    print >> sys.stderr, "Error: %s" % str(e)
    result = 1
  sys.exit(result)

# vim: ts=2:sw=2:tw=80:et:

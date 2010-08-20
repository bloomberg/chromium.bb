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

"""Generic utils."""

import errno
import logging
import os
import re
import stat
import subprocess
import sys
import threading
import time
import xml.dom.minidom
import xml.parsers.expat


class CheckCallError(OSError):
  """CheckCall() returned non-0."""
  def __init__(self, command, cwd, retcode, stdout, stderr=None):
    OSError.__init__(self, command, cwd, retcode, stdout, stderr)
    self.command = command
    self.cwd = cwd
    self.retcode = retcode
    self.stdout = stdout
    self.stderr = stderr


def Popen(*args, **kwargs):
  # *Sigh*:  Windows needs shell=True, or else it won't search %PATH% for the
  # executable, but shell=True makes subprocess on Linux fail when it's called
  # with a list because it only tries to execute the first item in the list.
  if not 'env' in kwargs:
    # It's easier to parse the stdout if it is always in English.
    kwargs['env'] = os.environ.copy()
    kwargs['env']['LANGUAGE'] = 'en'
  return subprocess.Popen(*args, shell=(sys.platform=='win32'), **kwargs)


def CheckCall(command, cwd=None, print_error=True):
  """Similar subprocess.check_call() but redirects stdout and
  returns (stdout, stderr).

  Works on python 2.4
  """
  logging.debug('%s, cwd=%s' % (str(command), str(cwd)))
  try:
    stderr = None
    if not print_error:
      stderr = subprocess.PIPE
    process = Popen(command, cwd=cwd, stdout=subprocess.PIPE, stderr=stderr)
    std_out, std_err = process.communicate()
  except OSError, e:
    raise CheckCallError(command, cwd, e.errno, None)
  if process.returncode:
    raise CheckCallError(command, cwd, process.returncode, std_out, std_err)
  return std_out, std_err


def SplitUrlRevision(url):
  """Splits url and returns a two-tuple: url, rev"""
  if url.startswith('ssh:'):
    # Make sure ssh://test@example.com/test.git@stable works
    regex = r'(ssh://(?:[\w]+@)?[-\w:\.]+/[-\w\./]+)(?:@(.+))?'
    components = re.search(regex, url).groups()
  else:
    components = url.split('@', 1)
    if len(components) == 1:
      components += [None]
  return tuple(components)


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


def SyntaxErrorToError(filename, e):
  """Raises a gclient_utils.Error exception with the human readable message"""
  try:
    # Try to construct a human readable error message
    if filename:
      error_message = 'There is a syntax error in %s\n' % filename
    else:
      error_message = 'There is a syntax error\n'
    error_message += 'Line #%s, character %s: "%s"' % (
        e.lineno, e.offset, re.sub(r'[\r\n]*$', '', e.text))
  except:
    # Something went wrong, re-raise the original exception
    raise e
  else:
    raise Error(error_message)


class PrintableObject(object):
  def __str__(self):
    output = ''
    for i in dir(self):
      if i.startswith('__'):
        continue
      output += '%s = %s\n' % (i, str(getattr(self, i, '')))
    return output


def FileRead(filename, mode='rU'):
  content = None
  f = open(filename, mode)
  try:
    content = f.read()
  finally:
    f.close()
  return content


def FileWrite(filename, content, mode='w'):
  f = open(filename, mode)
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
  logging.debug(path)
  file_path = os.path.join(*path)
  if not os.path.exists(file_path):
    return

  if os.path.islink(file_path) or not os.path.isdir(file_path):
    raise Error('RemoveDirectory asked to remove non-directory %s' % file_path)

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
                            fail_status=None, filter_fn=None):
  """Runs command, a list, in directory in_directory.

  If print_messages is true, a message indicating what is being done
  is printed to stdout. If print_messages is false, the message is printed
  only if we actually need to print something else as well, so you can
  get the context of the output. If print_messages is false and print_stdout
  is false, no output at all is generated.

  Also, if print_stdout is true, the command's stdout is also forwarded
  to stdout.

  If a filter_fn function is specified, it is expected to take a single
  string argument, and it will be called with each line of the
  subprocess's output. Each line has had the trailing newline character
  trimmed.

  If the command fails, as indicated by a nonzero exit status, gclient will
  exit with an exit status of fail_status.  If fail_status is None (the
  default), gclient will raise an Error exception.
  """
  logging.debug(command)
  if print_messages:
    print('\n________ running \'%s\' in \'%s\''
          % (' '.join(command), in_directory))

  kid = Popen(command, bufsize=0, cwd=in_directory,
              stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

  # Do a flush of sys.stdout before we begin reading from the subprocess's
  # stdout.
  last_flushed_at = time.time()
  sys.stdout.flush()

  # Also, we need to forward stdout to prevent weird re-ordering of output.
  # This has to be done on a per byte basis to make sure it is not buffered:
  # normally buffering is done for each line, but if svn requests input, no
  # end-of-line character is output after the prompt and it would not show up.
  in_byte = kid.stdout.read(1)
  in_line = ''
  while in_byte:
    if in_byte != '\r':
      if print_stdout:
        if not print_messages:
          print('\n________ running \'%s\' in \'%s\''
              % (' '.join(command), in_directory))
          print_messages = True
        sys.stdout.write(in_byte)
      if in_byte != '\n':
        in_line += in_byte
    if in_byte == '\n':
      if filter_fn:
        filter_fn(in_line)
      in_line = ''
      # Flush at least 10 seconds between line writes.  We wait at least 10
      # seconds to avoid overloading the reader that called us with output,
      # which can slow busy readers down.
      if (time.time() - last_flushed_at) > 10:
        last_flushed_at = time.time()
        sys.stdout.flush()
    in_byte = kid.stdout.read(1)
  rv = kid.wait()

  if rv:
    msg = 'failed to run command: %s' % ' '.join(command)

    if fail_status != None:
      print >> sys.stderr, msg
      sys.exit(fail_status)

    raise Error(msg)


def FindGclientRoot(from_dir, filename='.gclient'):
  """Tries to find the gclient root."""
  path = os.path.realpath(from_dir)
  while not os.path.exists(os.path.join(path, filename)):
    split_path = os.path.split(path)
    if not split_path[1]:
      return None
    path = split_path[0]
  logging.info('Found gclient root at ' + path)
  return path


def PathDifference(root, subpath):
  """Returns the difference subpath minus root."""
  root = os.path.realpath(root)
  subpath = os.path.realpath(subpath)
  if not subpath.startswith(root):
    return None
  # If the root does not have a trailing \ or /, we add it so the returned
  # path starts immediately after the seperator regardless of whether it is
  # provided.
  root = os.path.join(root, '')
  return subpath[len(root):]


def FindFileUpwards(filename, path=None):
  """Search upwards from the a directory (default: current) to find a file."""
  if not path:
    path = os.getcwd()
  path = os.path.realpath(path)
  while True:
    file_path = os.path.join(path, filename)
    if os.path.isfile(file_path):
      return file_path
    (new_path, _) = os.path.split(path)
    if new_path == path:
      return None
    path = new_path


def GetGClientRootAndEntries(path=None):
  """Returns the gclient root and the dict of entries."""
  config_file = '.gclient_entries'
  config_path = FindFileUpwards(config_file, path)

  if not config_path:
    print "Can't find %s" % config_file
    return None

  env = {}
  execfile(config_path, env)
  config_dir = os.path.dirname(config_path)
  return config_dir, env['entries']


class WorkItem(object):
  """One work item."""
  # A list of string, each being a WorkItem name.
  requirements = []
  # A unique string representing this work item.
  name = None

  def run(self):
    pass


class ExecutionQueue(object):
  """Runs a set of WorkItem that have interdependencies and were WorkItem are
  added as they are processed.

  In gclient's case, Dependencies sometime needs to be run out of order due to
  From() keyword. This class manages that all the required dependencies are run
  before running each one.

  Methods of this class are thread safe.
  """
  def __init__(self, jobs, progress):
    """jobs specifies the number of concurrent tasks to allow. progress is a
    Progress instance."""
    # Set when a thread is done or a new item is enqueued.
    self.ready_cond = threading.Condition()
    # Maximum number of concurrent tasks.
    self.jobs = jobs
    # List of WorkItem, for gclient, these are Dependency instances.
    self.queued = []
    # List of strings representing each Dependency.name that was run.
    self.ran = []
    # List of items currently running.
    self.running = []
    # Exceptions thrown if any.
    self.exceptions = []
    self.progress = progress
    if self.progress:
      self.progress.update()

  def enqueue(self, d):
    """Enqueue one Dependency to be executed later once its requirements are
    satisfied.
    """
    assert isinstance(d, WorkItem)
    self.ready_cond.acquire()
    try:
      self.queued.append(d)
      total = len(self.queued) + len(self.ran) + len(self.running)
      logging.debug('enqueued(%s)' % d.name)
      if self.progress:
        self.progress._total = total + 1
        self.progress.update(0)
      self.ready_cond.notifyAll()
    finally:
      self.ready_cond.release()

  def flush(self, *args, **kwargs):
    """Runs all enqueued items until all are executed."""
    self.ready_cond.acquire()
    try:
      while True:
        # Check for task to run first, then wait.
        while True:
          if self.exceptions:
            # Systematically flush the queue when there is an exception logged
            # in.
            self.queued = []
          # Flush threads that have terminated.
          self.running = [t for t in self.running if t.isAlive()]
          if not self.queued and not self.running:
            break
          if self.jobs == len(self.running):
            break
          for i in xrange(len(self.queued)):
            # Verify its requirements.
            for r in self.queued[i].requirements:
              if not r in self.ran:
                # Requirement not met.
                break
            else:
              # Start one work item: all its requirements are satisfied.
              d = self.queued.pop(i)
              new_thread = self._Worker(self, d, args=args, kwargs=kwargs)
              if self.jobs > 1:
                # Start the thread.
                self.running.append(new_thread)
                new_thread.start()
              else:
                # Run the 'thread' inside the main thread.
                new_thread.run()
              break
          else:
            # Couldn't find an item that could run. Break out the outher loop.
            break
        if not self.queued and not self.running:
          break
        # We need to poll here otherwise Ctrl-C isn't processed.
        self.ready_cond.wait(10)
        # Something happened: self.enqueue() or a thread terminated. Loop again.
    finally:
      self.ready_cond.release()
    assert not self.running, 'Now guaranteed to be single-threaded'
    if self.exceptions:
      # To get back the stack location correctly, the raise a, b, c form must be
      # used, passing a tuple as the first argument doesn't work.
      e = self.exceptions.pop(0)
      raise e[0], e[1], e[2]
    if self.progress:
      self.progress.end()

  class _Worker(threading.Thread):
    """One thread to execute one WorkItem."""
    def __init__(self, parent, item, args=(), kwargs=None):
      threading.Thread.__init__(self, name=item.name or 'Worker')
      self.args = args
      self.kwargs = kwargs or {}
      self.item = item
      self.parent = parent

    def run(self):
      """Runs in its own thread."""
      logging.debug('running(%s)' % self.item.name)
      exception = None
      try:
        self.item.run(*self.args, **self.kwargs)
      except Exception:
        # Catch exception location.
        exception = sys.exc_info()

      # This assumes the following code won't throw an exception. Bad.
      self.parent.ready_cond.acquire()
      try:
        if exception:
          self.parent.exceptions.append(exception)
        if self.parent.progress:
          self.parent.progress.update(1)
        assert not self.item.name in self.parent.ran
        if not self.item.name in self.parent.ran:
          self.parent.ran.append(self.item.name)
      finally:
        self.parent.ready_cond.notifyAll()
        self.parent.ready_cond.release()

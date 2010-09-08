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


class Error(Exception):
  """gclient exception class."""
  pass


class CheckCallError(OSError, Error):
  """CheckCall() returned non-0."""
  def __init__(self, command, cwd, returncode, stdout, stderr=None):
    OSError.__init__(self, command, cwd, returncode, stdout, stderr)
    Error.__init__(self)
    self.command = command
    self.cwd = cwd
    self.returncode = returncode
    self.stdout = stdout
    self.stderr = stderr

  def __str__(self):
    out = ' '.join(self.command)
    if self.cwd:
      out += ' in ' + self.cwd
    if self.returncode is not None:
      out += ' returned %d' % self.returncode
    if self.stdout is not None:
      out += '\nstdout: %s\n' % self.stdout
    if self.stderr is not None:
      out += '\nstderr: %s\n' % self.stderr
    return out


def Popen(args, **kwargs):
  """Calls subprocess.Popen() with hacks to work around certain behaviors.

  Ensure English outpout for svn and make it work reliably on Windows.
  """
  logging.debug(u'%s, cwd=%s' % (u' '.join(args), kwargs.get('cwd', '')))
  if not 'env' in kwargs:
    # It's easier to parse the stdout if it is always in English.
    kwargs['env'] = os.environ.copy()
    kwargs['env']['LANGUAGE'] = 'en'
  if not 'shell' in kwargs:
    # *Sigh*:  Windows needs shell=True, or else it won't search %PATH% for the
    # executable, but shell=True makes subprocess on Linux fail when it's called
    # with a list because it only tries to execute the first item in the list.
    kwargs['shell'] = (sys.platform=='win32')
  return subprocess.Popen(args, **kwargs)


def CheckCall(command, cwd=None, print_error=True):
  """Similar subprocess.check_call() but redirects stdout and
  returns (stdout, stderr).

  Works on python 2.4
  """
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


def CheckCallAndFilterAndHeader(args, always=False, **kwargs):
  """Adds 'header' support to CheckCallAndFilter.

  If |always| is True, a message indicating what is being done
  is printed to stdout all the time even if not output is generated. Otherwise
  the message header is printed only if the call generated any ouput.
  """
  stdout = kwargs.get('stdout', None) or sys.stdout
  if always:
    stdout.write('\n________ running \'%s\' in \'%s\'\n'
        % (' '.join(args), kwargs.get('cwd', '.')))
  else:
    filter_fn = kwargs.get('filter_fn', None)
    def filter_msg(line):
      if line is None:
        stdout.write('\n________ running \'%s\' in \'%s\'\n'
            % (' '.join(args), kwargs.get('cwd', '.')))
      elif filter_fn:
        filter_fn(line)
    kwargs['filter_fn'] = filter_msg
    kwargs['call_filter_on_first_line'] = True
  # Obviously.
  kwargs['print_stdout'] = True
  return CheckCallAndFilter(args, **kwargs)


class StdoutAutoFlush(object):
  """Automatically flush after N seconds."""
  def __init__(self, stdout, delay=10):
    self.lock = threading.Lock()
    self.stdout = stdout
    self.delay = delay
    self.last_flushed_at = time.time()
    self.stdout.flush()

  def write(self, out):
    """Thread-safe."""
    self.stdout.write(out)
    should_flush = False
    self.lock.acquire()
    try:
      if (time.time() - self.last_flushed_at) > self.delay:
        should_flush = True
        self.last_flushed_at = time.time()
    finally:
      self.lock.release()
    if should_flush:
      self.stdout.flush()

  def flush(self):
    self.stdout.flush()


def CheckCallAndFilter(args, stdout=None, filter_fn=None,
                       print_stdout=None, call_filter_on_first_line=False,
                       **kwargs):
  """Runs a command and calls back a filter function if needed.

  Accepts all subprocess.Popen() parameters plus:
    print_stdout: If True, the command's stdout is forwarded to stdout.
    filter_fn: A function taking a single string argument called with each line
               of the subprocess's output. Each line has the trailing newline
               character trimmed.
    stdout: Can be any bufferable output.

  stderr is always redirected to stdout.
  """
  assert print_stdout or filter_fn
  stdout = stdout or sys.stdout
  filter_fn = filter_fn or (lambda x: None)
  assert not 'stderr' in kwargs
  kid = Popen(args, bufsize=0,
              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
              **kwargs)

  # Do a flush of stdout before we begin reading from the subprocess's stdout
  stdout.flush()

  # Also, we need to forward stdout to prevent weird re-ordering of output.
  # This has to be done on a per byte basis to make sure it is not buffered:
  # normally buffering is done for each line, but if svn requests input, no
  # end-of-line character is output after the prompt and it would not show up.
  in_byte = kid.stdout.read(1)
  if in_byte:
    if call_filter_on_first_line:
      filter_fn(None)
    in_line = ''
    while in_byte:
      if in_byte != '\r':
        if print_stdout:
          stdout.write(in_byte)
        if in_byte != '\n':
          in_line += in_byte
        else:
          filter_fn(in_line)
          in_line = ''
      in_byte = kid.stdout.read(1)
    # Flush the rest of buffered output. This is only an issue with
    # stdout/stderr not ending with a \n.
    if len(in_line):
      filter_fn(in_line)
  rv = kid.wait()
  if rv:
    raise CheckCallError(args, kwargs.get('cwd', None), rv, None)
  return 0


def FindGclientRoot(from_dir, filename='.gclient'):
  """Tries to find the gclient root."""
  real_from_dir = os.path.realpath(from_dir)
  path = real_from_dir
  while not os.path.exists(os.path.join(path, filename)):
    split_path = os.path.split(path)
    if not split_path[1]:
      return None
    path = split_path[0]

  # If we did not find the file in the current directory, make sure we are in a
  # sub directory that is controlled by this configuration.
  if path != real_from_dir:
    entries_filename = os.path.join(path, filename + '_entries')
    if not os.path.exists(entries_filename):
      # If .gclient_entries does not exist, a previous call to gclient sync
      # might have failed. In that case, we cannot verify that the .gclient
      # is the one we want to use. In order to not to cause too much trouble,
      # just issue a warning and return the path anyway.
      print >>sys.stderr, ("%s file in parent directory %s might not be the "
          "file you want to use" % (filename, path))
      return path
    scope = {}
    try:
      exec(FileRead(entries_filename), scope)
    except SyntaxError, e:
      SyntaxErrorToError(filename, e)
    all_directories = scope['entries'].keys()
    path_to_check = real_from_dir[len(path)+1:]
    while path_to_check:
      if path_to_check in all_directories:
        return path
      path_to_check = os.path.dirname(path_to_check)
    return None
    
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

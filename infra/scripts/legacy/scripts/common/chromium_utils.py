# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Set of basic operations/utilities that are used by the build. """

from contextlib import contextmanager
import ast
import cStringIO
import copy
import errno
import fnmatch
import glob
import json
import os
import re
import shutil
import socket
import stat
import subprocess
import sys
import threading
import time
import traceback

from common import env


BUILD_DIR = os.path.realpath(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir))


# Local errors.
class MissingArgument(Exception):
  pass
class PathNotFound(Exception):
  pass
class ExternalError(Exception):
  pass

def IsWindows():
  return sys.platform == 'cygwin' or sys.platform.startswith('win')

def IsLinux():
  return sys.platform.startswith('linux')

def IsMac():
  return sys.platform.startswith('darwin')


def RemoveFile(*path):
  """Removes the file located at 'path', if it exists."""
  file_path = os.path.join(*path)
  try:
    os.remove(file_path)
  except OSError, e:
    if e.errno != errno.ENOENT:
      raise


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
  """
  file_path = os.path.join(*path)
  if not os.path.exists(file_path):
    return

  if sys.platform == 'win32':
    # Give up and use cmd.exe's rd command.
    file_path = os.path.normcase(file_path)
    for _ in xrange(3):
      print 'RemoveDirectory running %s' % (' '.join(
          ['cmd.exe', '/c', 'rd', '/q', '/s', file_path]))
      if not subprocess.call(['cmd.exe', '/c', 'rd', '/q', '/s', file_path]):
        break
      print '  Failed'
      time.sleep(3)
    return

  def RemoveWithRetry_non_win(rmfunc, path):
    if os.path.islink(path):
      return os.remove(path)
    else:
      return rmfunc(path)

  remove_with_retry = RemoveWithRetry_non_win

  def RmTreeOnError(function, path, excinfo):
    r"""This works around a problem whereby python 2.x on Windows has no ability
    to check for symbolic links.  os.path.islink always returns False.  But
    shutil.rmtree will fail if invoked on a symbolic link whose target was
    deleted before the link.  E.g., reproduce like this:
    > mkdir test
    > mkdir test\1
    > mklink /D test\current test\1
    > python -c "import chromium_utils; chromium_utils.RemoveDirectory('test')"
    To avoid this issue, we pass this error-handling function to rmtree.  If
    we see the exact sort of failure, we ignore it.  All other failures we re-
    raise.
    """

    exception_type = excinfo[0]
    exception_value = excinfo[1]
    # If shutil.rmtree encounters a symbolic link on Windows, os.listdir will
    # fail with a WindowsError exception with an ENOENT errno (i.e., file not
    # found).  We'll ignore that error.  Note that WindowsError is not defined
    # for non-Windows platforms, so we use OSError (of which it is a subclass)
    # to avoid lint complaints about an undefined global on non-Windows
    # platforms.
    if (function is os.listdir) and issubclass(exception_type, OSError):
      if exception_value.errno == errno.ENOENT:
        # File does not exist, and we're trying to delete, so we can ignore the
        # failure.
        print 'WARNING:  Failed to list %s during rmtree.  Ignoring.\n' % path
      else:
        raise
    else:
      raise

  for root, dirs, files in os.walk(file_path, topdown=False):
    # For POSIX:  making the directory writable guarantees removability.
    # Windows will ignore the non-read-only bits in the chmod value.
    os.chmod(root, 0770)
    for name in files:
      remove_with_retry(os.remove, os.path.join(root, name))
    for name in dirs:
      remove_with_retry(lambda p: shutil.rmtree(p, onerror=RmTreeOnError),
                        os.path.join(root, name))

  remove_with_retry(os.rmdir, file_path)


def FindUpwardParent(start_dir, *desired_list):
  """Finds the desired object's parent, searching upward from the start_dir.

  Searches within start_dir and within all its parents looking for the desired
  directory or file, which may be given in one or more path components. Returns
  the first directory in which the top desired path component was found, or
  raises PathNotFound if it wasn't.
  """
  desired_path = os.path.join(*desired_list)
  last_dir = ''
  cur_dir = start_dir
  found_path = os.path.join(cur_dir, desired_path)
  while not os.path.exists(found_path):
    last_dir = cur_dir
    cur_dir = os.path.dirname(cur_dir)
    if last_dir == cur_dir:
      raise PathNotFound('Unable to find %s above %s' %
                         (desired_path, start_dir))
    found_path = os.path.join(cur_dir, desired_path)
  # Strip the entire original desired path from the end of the one found
  # and remove a trailing path separator, if present.
  found_path = found_path[:len(found_path) - len(desired_path)]
  if found_path.endswith(os.sep):
    found_path = found_path[:len(found_path) - 1]
  return found_path


def FindUpward(start_dir, *desired_list):
  """Returns a path to the desired directory or file, searching upward.

  Searches within start_dir and within all its parents looking for the desired
  directory or file, which may be given in one or more path components. Returns
  the full path to the desired object, or raises PathNotFound if it wasn't
  found.
  """
  parent = FindUpwardParent(start_dir, *desired_list)
  return os.path.join(parent, *desired_list)


class RunCommandFilter(object):
  """Class that should be subclassed to provide a filter for RunCommand."""
  # Method could be a function
  # pylint: disable=R0201

  def FilterLine(self, a_line):
    """Called for each line of input.  The \n is included on a_line.  Should
    return what is to be recorded as the output for this line.  A result of
    None suppresses the line."""
    return a_line

  def FilterDone(self, last_bits):
    """Acts just like FilterLine, but is called with any data collected after
    the last newline of the command."""
    return last_bits


def RunCommand(command, parser_func=None, filter_obj=None, pipes=None,
               print_cmd=True, timeout=None, max_time=None, **kwargs):
  """Runs the command list, printing its output and returning its exit status.

  Prints the given command (which should be a list of one or more strings),
  then runs it and writes its stdout and stderr to the appropriate file handles.

  If timeout is set, the process will be killed if output is stopped after
  timeout seconds. If max_time is set, the process will be killed if it runs for
  more than max_time.

  If parser_func is not given, the subprocess's output is passed to stdout
  and stderr directly.  If the func is given, each line of the subprocess's
  stdout/stderr is passed to the func and then written to stdout.

  If filter_obj is given, all output is run through the filter a line
  at a time before it is written to stdout.

  We do not currently support parsing stdout and stderr independent of
  each other.  In previous attempts, this led to output ordering issues.
  By merging them when either needs to be parsed, we avoid those ordering
  issues completely.

  pipes is a list of commands (also a list) that will receive the output of
  the intial command. For example, if you want to run "python a | python b | c",
  the "command" will be set to ['python', 'a'], while pipes will be set to
  [['python', 'b'],['c']]
  """

  def TimedFlush(timeout, fh, kill_event):
    """Flush fh every timeout seconds until kill_event is true."""
    while True:
      try:
        fh.flush()
      # File handle is closed, exit.
      except ValueError:
        break
      # Wait for kill signal or timeout.
      if kill_event.wait(timeout):
        break

  # TODO(all): nsylvain's CommandRunner in buildbot_slave is based on this
  # method.  Update it when changes are introduced here.
  def ProcessRead(readfh, writefh, parser_func=None, filter_obj=None,
                  log_event=None):
    writefh.flush()

    # Python on Windows writes the buffer only when it reaches 4k.  Ideally
    # we would flush a minimum of 10 seconds.  However, we only write and
    # flush no more often than 20 seconds to avoid flooding the master with
    # network traffic from unbuffered output.
    kill_event = threading.Event()
    flush_thread = threading.Thread(
        target=TimedFlush, args=(20, writefh, kill_event))
    flush_thread.daemon = True
    flush_thread.start()

    try:
      in_byte = readfh.read(1)
      in_line = cStringIO.StringIO()
      while in_byte:
        # Capture all characters except \r.
        if in_byte != '\r':
          in_line.write(in_byte)

        # Write and flush on newline.
        if in_byte == '\n':
          if log_event:
            log_event.set()
          if parser_func:
            parser_func(in_line.getvalue().strip())

          if filter_obj:
            filtered_line = filter_obj.FilterLine(in_line.getvalue())
            if filtered_line is not None:
              writefh.write(filtered_line)
          else:
            writefh.write(in_line.getvalue())
          in_line = cStringIO.StringIO()
        in_byte = readfh.read(1)

      if log_event and in_line.getvalue():
        log_event.set()

      # Write remaining data and flush on EOF.
      if parser_func:
        parser_func(in_line.getvalue().strip())

      if filter_obj:
        if in_line.getvalue():
          filtered_line = filter_obj.FilterDone(in_line.getvalue())
          if filtered_line is not None:
            writefh.write(filtered_line)
      else:
        if in_line.getvalue():
          writefh.write(in_line.getvalue())
    finally:
      kill_event.set()
      flush_thread.join()
      writefh.flush()

  pipes = pipes or []

  # Print the given command (which should be a list of one or more strings).
  if print_cmd:
    print '\n' + subprocess.list2cmdline(command) + '\n',
    for pipe in pipes:
      print '     | ' + subprocess.list2cmdline(pipe) + '\n',

  sys.stdout.flush()
  sys.stderr.flush()

  if not (parser_func or filter_obj or pipes or timeout or max_time):
    # Run the command.  The stdout and stderr file handles are passed to the
    # subprocess directly for writing.  No processing happens on the output of
    # the subprocess.
    proc = subprocess.Popen(command, stdout=sys.stdout, stderr=sys.stderr,
                            bufsize=0, **kwargs)

  else:
    if not (parser_func or filter_obj):
      filter_obj = RunCommandFilter()

    # Start the initial process.
    proc = subprocess.Popen(command, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, bufsize=0, **kwargs)
    proc_handles = [proc]

    if pipes:
      pipe_number = 0
      for pipe in pipes:
        pipe_number = pipe_number + 1
        if pipe_number == len(pipes) and not (parser_func or filter_obj):
          # The last pipe process needs to output to sys.stdout or filter
          stdout = sys.stdout
        else:
          # Output to a pipe, since another pipe is on top of us.
          stdout = subprocess.PIPE
        pipe_proc = subprocess.Popen(pipe, stdin=proc_handles[0].stdout,
                                     stdout=stdout, stderr=subprocess.STDOUT)
        proc_handles.insert(0, pipe_proc)

      # Allow proc to receive a SIGPIPE if the piped process exits.
      for handle in proc_handles[1:]:
        handle.stdout.close()

    log_event = threading.Event()

    # Launch and start the reader thread.
    thread = threading.Thread(target=ProcessRead,
                              args=(proc_handles[0].stdout, sys.stdout),
                              kwargs={'parser_func': parser_func,
                                      'filter_obj': filter_obj,
                                      'log_event': log_event})

    kill_lock = threading.Lock()


    def term_then_kill(handle, initial_timeout, numtimeouts, interval):
      def timed_check():
        for _ in range(numtimeouts):
          if handle.poll() is not None:
            return True
          time.sleep(interval)

      handle.terminate()
      time.sleep(initial_timeout)
      timed_check()
      if handle.poll() is None:
        handle.kill()
      timed_check()
      return handle.poll() is not None


    def kill_proc(proc_handles, message=None):
      with kill_lock:
        if proc_handles:
          killed = term_then_kill(proc_handles[0], 0.1, 5, 1)

          if message:
            print >> sys.stderr, message

          if not killed:
            print >> sys.stderr, 'could not kill pid %d!' % proc_handles[0].pid
          else:
            print >> sys.stderr, 'program finished with exit code %d' % (
                proc_handles[0].returncode)

          # Prevent other timeouts from double-killing.
          del proc_handles[:]

    def timeout_func(timeout, proc_handles, log_event, finished_event):
      while log_event.wait(timeout):
        log_event.clear()
        if finished_event.is_set():
          return

      message = ('command timed out: %d seconds without output, attempting to '
                 'kill' % timeout)
      kill_proc(proc_handles, message)

    def maxtimeout_func(timeout, proc_handles, finished_event):
      if not finished_event.wait(timeout):
        message = ('command timed out: %d seconds elapsed' % timeout)
        kill_proc(proc_handles, message)

    timeout_thread = None
    maxtimeout_thread = None
    finished_event = threading.Event()

    if timeout:
      timeout_thread = threading.Thread(target=timeout_func,
                                        args=(timeout, proc_handles, log_event,
                                              finished_event))
      timeout_thread.daemon = True
    if max_time:
      maxtimeout_thread = threading.Thread(target=maxtimeout_func,
                                           args=(max_time, proc_handles,
                                                 finished_event))
      maxtimeout_thread.daemon = True

    thread.start()
    if timeout_thread:
      timeout_thread.start()
    if maxtimeout_thread:
      maxtimeout_thread.start()

    # Wait for the commands to terminate.
    for handle in proc_handles:
      handle.wait()

    # Wake up timeout threads.
    finished_event.set()
    log_event.set()

    # Wait for the reader thread to complete (implies EOF reached on stdout/
    # stderr pipes).
    thread.join()

    # Check whether any of the sub commands has failed.
    for handle in proc_handles:
      if handle.returncode:
        return handle.returncode

  # Wait for the command to terminate.
  proc.wait()
  return proc.returncode


def GetStatusOutput(command, **kwargs):
  """Runs the command list, returning its result and output."""
  proc = subprocess.Popen(command, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, bufsize=1,
                          **kwargs)
  output = proc.communicate()[0]
  result = proc.returncode

  return (result, output)


def GetCommandOutput(command):
  """Runs the command list, returning its output.

  Run the command and returns its output (stdout and stderr) as a string.

  If the command exits with an error, raises ExternalError.
  """
  (result, output) = GetStatusOutput(command)
  if result:
    raise ExternalError('%s: %s' % (subprocess.list2cmdline(command), output))
  return output


def ListMasters(cue='master.cfg', include_public=True, include_internal=True):
  """Returns all the masters found."""
  # Look for "internal" masters first.
  path_internal = os.path.join(
      BUILD_DIR, os.pardir, 'build_internal', 'masters/*/' + cue)
  path = os.path.join(BUILD_DIR, 'masters/*/' + cue)
  filenames = []
  if include_public:
    filenames += glob.glob(path)
  if include_internal:
    filenames += glob.glob(path_internal)
  return [os.path.abspath(os.path.dirname(f)) for f in filenames]


def MasterPath(mastername, include_public=True, include_internal=True):
  path = os.path.join(BUILD_DIR, 'masters', 'master.%s' % mastername)
  path_internal = os.path.join(
      BUILD_DIR, os.pardir, 'build_internal', 'masters',
      'master.%s' % mastername)
  if include_public and os.path.isdir(path):
    return path
  if include_internal and os.path.isdir(path_internal):
    return path_internal
  raise LookupError('Path for master %s not found' % mastername)


def ListMastersWithSlaves(include_public=True, include_internal=True):
  masters_path = ListMasters('builders.pyl', include_public, include_internal)
  masters_path.extend(ListMasters('slaves.cfg', include_public,
                                  include_internal))
  return masters_path


def GetSlavesFromMasterPath(path, fail_hard=False):
  builders_path = os.path.join(path, 'builders.pyl')
  if os.path.exists(builders_path):
    return GetSlavesFromBuildersFile(builders_path)
  return RunSlavesCfg(os.path.join(path, 'slaves.cfg'), fail_hard=fail_hard)


def GetAllSlaves(fail_hard=False, include_public=True, include_internal=True):
  """Return all slave objects from masters."""
  slaves = []
  for master in ListMastersWithSlaves(include_public, include_internal):
    cur_slaves = GetSlavesFromMasterPath(master, fail_hard)
    for slave in cur_slaves:
      slave['mastername'] = os.path.basename(master)
    slaves.extend(cur_slaves)
  return slaves


def GetActiveSubdir():
  """Get current checkout's subdir, if checkout uses subdir layout."""
  rootdir, subdir = os.path.split(os.path.dirname(BUILD_DIR))
  if subdir != 'b' and os.path.basename(rootdir) == 'c':
    return subdir


def GetActiveSlavename():
  slavename = os.getenv('TESTING_SLAVENAME')
  if not slavename:
    slavename = socket.getfqdn().split('.', 1)[0].lower()
  subdir = GetActiveSubdir()
  if subdir:
    return '%s#%s' % (slavename, subdir)
  return slavename


def EntryToSlaveName(entry):
  """Produces slave name from the slaves config dict."""
  name = entry.get('slavename') or entry.get('hostname')
  if 'subdir' in entry:
    return '%s#%s' % (name, entry['subdir'])
  return name


def GetActiveMaster(slavename=None, default=None):
  """Returns the name of the Active master serving the current host.

  Parse all of the active masters with slaves matching the current hostname
  and optional slavename. Returns |default| if no match found.
  """
  slavename = slavename or GetActiveSlavename()
  for slave in GetAllSlaves():
    if slavename == EntryToSlaveName(slave):
      return slave['master']
  return default


@contextmanager
def MasterEnvironment(master_dir):
  """Context manager that enters an enviornment similar to a master's.

  This involves:
    - Modifying 'sys.path' to include paths available to the master.
    - Changing directory (via os.chdir()) to the master's base directory.

  These changes will be reverted after the context manager completes.

  Args:
    master_dir: (str) The master's base directory.
  """
  master_dir = os.path.abspath(master_dir)

  # Setup a 'sys.path' that is adequate for loading 'slaves.cfg'.
  old_cwd = os.getcwd()

  with env.GetInfraPythonPath(master_dir=master_dir).Enter():
    try:
      os.chdir(master_dir)
      yield
    finally:
      os.chdir(old_cwd)


def ParsePythonCfg(cfg_filepath, fail_hard=False):
  """Retrieves data from a python config file."""
  if not os.path.exists(cfg_filepath):
    return None

  # Execute 'slaves.sfg' in the master path environment.
  with MasterEnvironment(os.path.dirname(os.path.abspath(cfg_filepath))):
    try:
      local_vars = {}
      execfile(os.path.join(cfg_filepath), local_vars)
      del local_vars['__builtins__']
      return local_vars
    except Exception as e:
      # pylint: disable=C0323
      print >>sys.stderr, 'An error occurred while parsing %s: %s' % (
          cfg_filepath, e)
      print >>sys.stderr, traceback.format_exc()  # pylint: disable=C0323
      if fail_hard:
        raise
      return {}


def RunSlavesCfg(slaves_cfg, fail_hard=False):
  """Runs slaves.cfg in a consistent way."""
  slave_config = ParsePythonCfg(slaves_cfg, fail_hard=fail_hard) or {}
  return slave_config.get('slaves', [])


def convert_json(option, _, value, parser):
  """Provide an OptionParser callback to unmarshal a JSON string."""
  setattr(parser.values, option.dest, json.loads(value))


def AddPropertiesOptions(option_parser):
  """Registers command line options for parsing build and factory properties.

  After parsing, the options object will have the 'build_properties' and
  'factory_properties' attributes. The corresponding values will be python
  dictionaries containing the properties. If the options are not given on
  the command line, the dictionaries will be empty.

  Args:
    option_parser: An optparse.OptionParser to register command line options
                   for build and factory properties.
  """
  option_parser.add_option('--build-properties', action='callback',
                           callback=convert_json, type='string',
                           nargs=1, default={},
                           help='build properties in JSON format')
  option_parser.add_option('--factory-properties', action='callback',
                           callback=convert_json, type='string',
                           nargs=1, default={},
                           help='factory properties in JSON format')


def ReadJsonAsUtf8(filename=None, text=None):
  """Read a json file or string and output a dict.

  This function is different from json.load and json.loads in that it
  returns utf8-encoded string for keys and values instead of unicode.

  Args:
  filename: path of a file to parse
  text: json string to parse

  If both 'filename' and 'text' are provided, 'filename' is used.
  """
  def _decode_list(data):
    rv = []
    for item in data:
      if isinstance(item, unicode):
        item = item.encode('utf-8')
      elif isinstance(item, list):
        item = _decode_list(item)
      elif isinstance(item, dict):
        item = _decode_dict(item)
      rv.append(item)
    return rv

  def _decode_dict(data):
    rv = {}
    for key, value in data.iteritems():
      if isinstance(key, unicode):
        key = key.encode('utf-8')
      if isinstance(value, unicode):
        value = value.encode('utf-8')
      elif isinstance(value, list):
        value = _decode_list(value)
      elif isinstance(value, dict):
        value = _decode_dict(value)
      rv[key] = value
    return rv

  if filename:
    with open(filename, 'rb') as f:
      return json.load(f, object_hook=_decode_dict)
  if text:
    return json.loads(text, object_hook=_decode_dict)


def DatabaseSetup(buildmaster_config, require_dbconfig=False):
  """Read database credentials in the master directory."""
  if os.path.isfile('.dbconfig'):
    values = {}
    execfile('.dbconfig', values)
    if 'password' not in values:
      raise Exception('could not get db password')

    buildmaster_config['db_url'] = 'postgresql://%s:%s@%s/%s' % (
        values['username'], values['password'],
        values.get('hostname', 'localhost'), values['dbname'])
  else:
    assert not require_dbconfig


def ReadBuildersFile(builders_path):
  with open(builders_path) as fp:
    contents = fp.read()
  return ParseBuildersFileContents(builders_path, contents)


def ParseBuildersFileContents(path, contents):
  builders = ast.literal_eval(contents)

  # Set some additional derived fields that are derived from the
  # file's location in the filesystem.
  basedir = os.path.dirname(os.path.abspath(path))
  master_dirname = os.path.basename(basedir)
  master_name_comps = master_dirname.split('.')[1:]
  buildbot_path =  '.'.join(master_name_comps)
  master_classname =  ''.join(c[0].upper() + c[1:] for c in master_name_comps)
  builders['master_dirname'] = master_dirname
  builders.setdefault('master_classname', master_classname)
  builders.setdefault('buildbot_url',
                      'https://build.chromium.org/p/%s/' % buildbot_path)

  builders.setdefault('buildbucket_bucket', None)
  builders.setdefault('service_account_file', None)

  # The _str fields are printable representations of Python values:
  # if builders['foo'] == "hello", then builders['foo_str'] == "'hello'".
  # This allows them to be read back in by Python scripts properly.
  builders['buildbucket_bucket_str'] = repr(builders['buildbucket_bucket'])
  builders['service_account_file_str'] = repr(builders['service_account_file'])

  return builders


def GetSlavesFromBuildersFile(builders_path):
  """Read builders_path and return a list of slave dicts."""
  builders = ReadBuildersFile(builders_path)
  return GetSlavesFromBuilders(builders)


def GetSlavesFromBuilders(builders):
  """Returns a list of slave dicts derived from the builders dict."""
  builders_in_pool = {}

  # builders.pyl contains a list of builders -> slave_pools
  # and a list of slave_pools -> slaves.
  # We require that each slave is in a single pool, but each slave
  # may have multiple builders, so we need to build up the list of
  # builders each slave pool supports.
  for builder_name, builder_vals in builders['builders'].items():
    pool_names = builder_vals['slave_pools']
    for pool_name in pool_names:
     if pool_name not in builders_in_pool:
       builders_in_pool[pool_name] = set()
     pool_data = builders['slave_pools'][pool_name]
     for slave in pool_data['slaves']:
       builders_in_pool[pool_name].add(builder_name)

  # Now we can generate the list of slaves using the above lookup table.
  slaves = []
  for pool_name, pool_data in builders['slave_pools'].items():
    slave_data = pool_data['slave_data']
    builder_names = sorted(builders_in_pool[pool_name])
    for slave in pool_data['slaves']:
      slaves.append({
          'hostname': slave,
          'builder_name': builder_names,
          'master': builders['master_classname'],
          'os': slave_data['os'],
          'version': slave_data['version'],
          'bits': slave_data['bits'],
      })

  return slaves

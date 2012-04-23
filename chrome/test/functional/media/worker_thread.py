# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Worker thread base class.

Worker threads are used to run multiple PyUITests simultaneously. They
synchronize calls to the browser."""

import itertools
import threading
import pyauto


# A static lock used to synchronize worker threads access to the browser.
__lock = threading.RLock()

def synchronized(fn):
  """A decorator to wrap a lock around function calls."""
  def syncFun(*args, **kwargs):
    with __lock:
      return fn(*args, **kwargs)

  return syncFun


def RunWorkerThreads(pyauto_test, test_worker_class, tasks, num_threads,
                     test_path):
  """Creates a matrix of tasks and starts test worker threads to run them.

  Args:
    pyauto_test: Reference to a pyauto.PyUITest instance.
    test_worker_class: WorkerThread class reference.
    tasks: Queue of tasks to run by the worker threads.
    num_threads: Number of threads to run simultaneously.
    test_path: Path to HTML/JavaScript test code.
  """
  # Convert relative test path into an absolute path.
  test_url = pyauto_test.GetFileURLForDataPath(test_path)

  # Add shutdown magic to end of queue.
  for _ in xrange(num_threads):
    tasks.put(None)

  threads = []
  for _ in xrange(num_threads):
    threads.append(test_worker_class(pyauto_test, tasks, test_url))

  # Wait for threads to exit, gracefully or otherwise.
  for thread in threads:
    thread.join()

  return sum(thread.failures for thread in threads)


class WorkerThread(threading.Thread):
  """Thread which for each queue task: opens tab, runs task, closes tab."""

  # Atomic, monotonically increasing task identifier.  Used to ID tabs.
  _task_id = itertools.count()

  def __init__(self, pyauto_test, tasks, url):
    """Sets up WorkerThread class variables.

    Args:
      pyauto_test: Reference to a pyauto.PyUITest instance.
      tasks: Queue containing task tuples used by RunTest().
      url: File URL to HTML/JavaScript test code.
    """
    threading.Thread.__init__(self)
    self.__pyauto = pyauto_test
    self.__tasks = tasks
    self.__url = url
    self.failures = 0
    self.start()

  def RunTask(self, unique_url, task):
    """Runs the specific task on the url test page.

    This method should be overridden to start the test on the unique_url page.

    Args:
      unique_url: A unique identifier of the test page.
      task: A tuple with information needed to run the test.
    Returns:
      True if the task finished as expected.
    """
    raise NotImplementedError('RunTask should be defined in a subclass.')

  def run(self):
    """For each task in queue: opens new tab, calls RunTask(), then closes tab.

    No exception handling is done to make sure the main thread exits properly
    during Chrome crashes or other failures.

    For a clean shutdown, put the magic exit value None in the queue.
    """
    while True:
      task = self.__tasks.get()
      # Check for magic exit values.
      if task is None:
        break
      # Make the test URL unique so we can figure out our tab index later.
      unique_url = '%s?%d' % (self.__url, WorkerThread._task_id.next())
      self.AppendTab(unique_url)
      if not self.RunTask(unique_url, task):
        self.failures += 1
      self.CloseTab(unique_url)
      self.__tasks.task_done()

  def __FindTabLocked(self, url):
    """Returns the tab index for the tab belonging to this url.

    __lock must be owned by caller.
    """
    if url is None:
      return 0
    for tab in self.__pyauto.GetBrowserInfo()['windows'][0]['tabs']:
      if tab['url'] == url:
        return tab['index']

  # The following are wrappers to pyauto.PyUITest functions. They are wrapped
  # with an internal lock to avoid problems when more than one thread edits the
  # state of the browser.
  #
  # We limit access of subclasses to the following functions. If subclasses
  # access other PyUITest functions, then there is no guarantee of thread
  # safety.
  #
  # For details on the args check pyauto.PyUITest class.
  @synchronized
  def AppendTab(self, url):
    self.__pyauto.AppendTab(pyauto.GURL(url))

  @synchronized
  def CallJavascriptFunc(self, fun_name, fun_args=[], url=None):
    return self.__pyauto.CallJavascriptFunc(fun_name, fun_args,
                                            tab_index=self.__FindTabLocked(url))

  @synchronized
  def CloseTab(self, url):
    """Closes the tab with the given url."""
    return self.__pyauto.GetBrowserWindow(0).GetTab(
        self.__FindTabLocked(url)).Close(True)

  @synchronized
  def GetDOMValue(self, name, url=None):
    return self.__pyauto.GetDOMValue(name, tab_index=self.__FindTabLocked(url))

  def WaitUntil(self, *args, **kwargs):
    """We do not need to lock WaitUntil since it does not call into Chrome.

    Ensure that the function passed in the args is thread safe.
    """
    return self.__pyauto.WaitUntil(*args, **kwargs)

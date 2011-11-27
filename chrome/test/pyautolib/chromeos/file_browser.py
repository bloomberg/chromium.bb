# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import simplejson as json  # found in third_party


class FileBrowser(object):
  """This class provides an API for automating the ChromeOS File Browser.

  Example:
    # Create and change into 'hello world' folder.
    executor = pyauto.PyUITest.JavascriptExecutorInTab(self)
    file_browser = chromeos.file_browser.FileBrowser(self, executor)
    if file_browser.WaitUntilInitialized():
      file_browser.CreateDirectory('hello world')
      file_browser.ChangeDirectory('hello world')

  For complete examples refer to chromeos_file_browser.py.
  """

  def __init__(self, ui_test, executor):
    """Initialize FileBrowser.

    Args:
      ui_test: derived from pyauto.PyUITest - base class for UI test cases.
      executor: derived from pyauto.PyUITest.JavascriptExecutor.
    """
    self._ui_test = ui_test
    self.executor = executor

  def Select(self, name):
    """Add entry with given name to the current selection.

    Args:
      name: Name of the entry to add to selection

    Returns:
       Whether entry exists.
    """
    script = """
      pyautoAPI.addItemToSelection('%s');
    """ % name
    return self.executor.Execute(script)

  def DirectoryContents(self):
    """Return a set containing all entries in the current directory.

    Returns:
      A set of entries.
    """
    script = """
      pyautoAPI.listDirectory();
    """
    list = json.loads(self.executor.Execute(script))
    return set(list)

  def Save(self, name):
    """Save the entry using the given name.

    Args:
      name: Name given to entry to be saved.
    """
    script = """
      pyautoAPI.saveItemAs('%s');
    """ % name
    self.executor.Execute(script)

  def Open(self):
    """Open selected entries."""
    script = """
      pyautoAPI.openItem();
    """
    self.executor.Execute(script)

  def Copy(self):
    """Copy selected entries to clipboard."""
    script = """
      pyautoAPI.copyItems();
    """
    self.executor.Execute(script)

  def Cut(self):
    """Cut selected entries to clipboard. """
    script = """
      pyautoAPI.cutItems();
    """
    self.executor.Execute(script)

  def Paste(self):
    """Paste entries from clipboard."""
    script = """
      pyautoAPI.pasteItems();
    """
    self.executor.Execute(script)

  def Rename(self, name):
    """Rename selected entry.

    Args:
      name: New name of the entry.
    """
    script = """
      pyautoAPI.renameItem('%s');
    """ % name
    self.executor.Execute(script)

  def Delete(self):
    """Delete selected entries."""
    script = """
      pyautoAPI.deleteItems();
    """
    self.executor.Execute(script)

  def CreateDirectory(self, name):
    """Create directory.

    Args:
      name: Name of the directory.
    """
    script = """
      pyautoAPI.createDirectory('%s');
    """ % name
    self.executor.Execute(script)

  def ChangeDirectory(self, path):
    """Change to a directory.

    A path starting with '/' is absolute, otherwise it is relative to the
    current directory.

    Args:
      name: Path to directory.
    """
    script = """
      pyautoAPI.changeDirectory('%s');
    """ % path
    self.executor.Execute(script)

  def CurrentDirectory(self):
    """Get the absolute path of current directory.

    Returns:
      Path to the current directory.
    """
    script = """
      pyautoAPI.currentDirectory();
    """
    return self.executor.Execute(script)

  def GetSelectedDirectorySizeStats(self):
    """Get remaining and total size of selected directory.

    Returns:
      A tuple: (remaining size in KB, total size in KB)
    """
    script = """
      pyautoAPI.getSelectedDirectorySizeStats();
    """
    stats = json.loads(self.executor.Execute(script))
    return stats['remainingSizeKB'], stats['totalSizeKB']

  def WaitUntilInitialized(self):
    """Returns whether the file manager is initialized.

    This should be called before calling any of the functions above.

    Returns:
      Whether file manager is initialied.
    """
    def _IsInitialized():
      script = """
        pyautoAPI.isInitialized();
      """
      return self.executor.Execute(script)
    return self._ui_test.WaitUntil(lambda: _IsInitialized())

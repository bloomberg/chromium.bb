#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""
Stess Tests for Google Chrome.

This script runs 4 different stress tests:
1. Plugin stress.
2. Back and forward stress.
3. Download stress.
4. Preference stress.

After every cycle (running all 4 stress tests) it checks for crashes.
If there are any crashes, the script generates a report, uploads it to
a server and mails about the crash and the link to the report on the server.
Apart from this whenever the test stops on mac it looks for and reports
zombies.

Prerequisites:
Test needs the following files/folders in the Data dir.
1. A crash_report tool in "pyauto_private/stress/mac" folder for use on Mac.
2. A "downloads" folder containing stress_downloads and all the files
   referenced in it.
3. A pref_dict file in "pyauto_private/stress/mac" folder.
4. A "plugin" folder containing doubleAnimation.xaml, flash.swf, FlashSpin.swf,
   generic.html, get_flash_player.gif, js-invoker.swf, mediaplayer.wmv,
   NavigatorTicker11.class, Plugins_page.html, sample5.mov, silverlight.xaml,
   silverlight.js, embed.pdf, plugins_page.html and test6.swf.
5. A stress_pref file in "pyauto_private/stress".
"""


import commands
import glob
import logging
import os
import random
import re
import shutil
import sys
import time
import urllib
import test_utils
import subprocess

import pyauto_functional
import pyauto
import pyauto_utils


CRASHES = 'crashes'  # Name of the folder to store crashes


class StressTest(pyauto.PyUITest):
  """Run all the stress tests."""

  flash_url1 = pyauto.PyUITest.GetFileURLForPath(
      os.path.join(pyauto.PyUITest.DataDir(), 'plugin', 'flash.swf'))
  flash_url2 = pyauto.PyUITest.GetFileURLForPath(
      os.path.join(pyauto.PyUITest.DataDir(),'plugin', 'js-invoker.swf'))
  flash_url3 = pyauto.PyUITest.GetFileURLForPath(
      os.path.join(pyauto.PyUITest.DataDir(), 'plugin', 'generic.html'))
  plugin_url = pyauto.PyUITest.GetFileURLForPath(
      os.path.join(pyauto.PyUITest.DataDir(), 'plugin', 'plugins_page.html'))
  empty_url = pyauto.PyUITest.GetFileURLForPath(
      os.path.join(pyauto.PyUITest.DataDir(), 'empty.html'))
  download_url1 = pyauto.PyUITest.GetFileURLForPath(
      os.path.join(pyauto.PyUITest.DataDir(), 'downloads', 'a_zip_file.zip'))
  download_url2 = pyauto.PyUITest.GetFileURLForPath(
      os.path.join(pyauto.PyUITest.DataDir(),'zip', 'test.zip'))
  file_list = pyauto.PyUITest.EvalDataFrom(
      os.path.join(pyauto.PyUITest.DataDir(), 'downloads', 'stress_downloads'))
  symbols_dir = os.path.join(os.getcwd(), 'Build_Symbols')
  stress_pref = pyauto.PyUITest.EvalDataFrom(
      os.path.join(pyauto.PyUITest.DataDir(), 'pyauto_private', 'stress',
                   'stress_pref'))
  breakpad_dir = None
  chrome_version = None
  bookmarks_list = []


  def _FuncDir(self):
    """Returns the path to the functional dir chrome/test/functional."""
    return os.path.dirname(__file__)

  def _DownloadSymbols(self):
    """Downloads the symbols for the build being tested."""
    download_location = os.path.join(os.getcwd(), 'Build_Symbols')
    if os.path.exists(download_location):
      shutil.rmtree(download_location)
    os.makedirs(download_location)

    url = self.stress_pref['symbols_dir'] + self.chrome_version
    # TODO: Add linux symbol_files
    if self.IsWin():
      url = url + '/win/'
      symbol_files = ['chrome_dll.pdb', 'chrome_exe.pdb']
    elif self.IsMac():
      url = url + '/mac/'
      symbol_files = map(urllib.quote,
                         ['Google Chrome Framework.framework',
                          'Google Chrome Helper.app',
                          'Google Chrome.app',
                          'crash_inspector',
                          'crash_report_sender',
                          'ffmpegsumo.so',
                          'libplugin_carbon_interpose.dylib'])
      index = 0
      symbol_files = ['%s-%s-i386.breakpad' % (sym_file, self.chrome_version) \
                      for sym_file in symbol_files]
      logging.info(symbol_files)

    for sym_file in symbol_files:
      sym_url = url + sym_file
      logging.info(sym_url)
      download_sym_file = os.path.join(download_location, sym_file)
      logging.info(download_sym_file)
      urllib.urlretrieve(sym_url, download_sym_file)

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self.breakpad_dir = self._CrashDumpFolder()
    self.chrome_version = self.GetBrowserInfo()['properties']['ChromeVersion']

  # Plugin stress functions

  def _CheckForPluginProcess(self, plugin_name):
    """Checks if a particular plugin process exists.

    Args:
      plugin_name : plugin process which should be running.
    """
    process = self.GetBrowserInfo()['child_processes']
    self.assertTrue([x for x in process
                     if x['type'] == 'Plug-in' and
                     x['name'] == plugin_name])

  def _GetPluginProcessId(self, plugin_name):
    """Get Plugin process id.

    Args:
      plugin_name: Plugin whose pid is expected.
                    Eg: "Shockwave Flash"

    Returns:
      Process id if the plugin process is running.
      None otherwise.
    """
    for process in self.GetBrowserInfo()['child_processes']:
      if process['type'] == 'Plug-in' and \
         re.search(plugin_name, process['name']):
        return process['pid']
    return None

  def _CloseAllTabs(self):
    """Close all but one tab in first window."""
    tab_count = self.GetTabCount(0)
    for tab_index in xrange(tab_count - 1, 0, -1):
      self.GetBrowserWindow(0).GetTab(tab_index).Close(True)

  def _CloseAllWindows(self):
    """Close all windows except one."""
    win_count = self.GetBrowserWindowCount()
    for windex in xrange(win_count - 1, 0, -1):
      self.RunCommand(pyauto.IDC_CLOSE_WINDOW, windex)

  def _ReloadAllTabs(self):
    """Reload all the tabs in first window."""
    for tab_index in range(self.GetTabCount()):
      self.GetBrowserWindow(0).GetTab(tab_index).Reload()

  def _LoadFlashInMultipleTabs(self):
    """Load Flash in multiple tabs in first window."""
    self.NavigateToURL(self.empty_url)
    # Open 18 tabs with flash
    for _ in range(9):
      self.AppendTab(pyauto.GURL(self.flash_url1))
      self.AppendTab(pyauto.GURL(self.flash_url2))

  def _OpenAndCloseMultipleTabsWithFlash(self):
    """Stress test for flash in multiple tabs."""
    logging.info("In _OpenAndCloseMultipleWindowsWithFlash.")
    self._LoadFlashInMultipleTabs()
    self._CheckForPluginProcess('Shockwave Flash')
    self._CloseAllTabs()

  def _OpenAndCloseMultipleWindowsWithFlash(self):
    """Stress test for flash in multiple windows."""
    logging.info('In _OpenAndCloseMultipleWindowsWithFlash.')
    # Open 5 Normal and 4 Incognito windows
    for tab_index in range(1, 10):
      if tab_index < 6:
        self.OpenNewBrowserWindow(True)
      else:
        self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
      self.NavigateToURL(self.flash_url2, tab_index, 0)
      self.AppendTab(pyauto.GURL(self.flash_url2), tab_index)
    self._CloseAllWindows()

  def _OpenAndCloseMultipleTabsWithMultiplePlugins(self):
    """Stress test using multiple plugins in multiple tabs."""
    logging.info('In _OpenAndCloseMultipleTabsWithMultiplePlugins.')
    # Append 4 tabs with URL
    for _ in range(5):
      self.AppendTab(pyauto.GURL(self.plugin_url))
    self._CloseAllTabs()

  def _OpenAndCloseMultipleWindowsWithMultiplePlugins(self):
    """Stress test using multiple plugins in multiple windows."""
    logging.info('In _OpenAndCloseMultipleWindowsWithMultiplePlugins.')
    # Open 4 windows with URL
    for tab_index in range(1, 5):
      if tab_index < 6:
        self.OpenNewBrowserWindow(True)
      else:
        self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
      self.NavigateToURL(self.plugin_url, tab_index, 0)
    self._CloseAllWindows()

  def _KillAndReloadFlash(self):
    """Stress test by killing flash process and reloading tabs."""
    self._LoadFlashInMultipleTabs()
    flash_process_id1 = self._GetPluginProcessId('Shockwave Flash')
    self.Kill(flash_process_id1)
    self._ReloadAllTabs()
    self._CloseAllTabs()

  def _KillAndReloadRenderersWithFlash(self):
    """Stress test by killing renderer processes and reloading tabs."""
    logging.info('In _KillAndReloadRenderersWithFlash')
    self._LoadFlashInMultipleTabs()
    info = self.GetBrowserInfo()
    # Kill all renderer processes
    for tab_index in range(self.GetTabCount(0)):
      self.KillRendererProcess(
          info['windows'][0]['tabs'][tab_index]['renderer_pid'])
    self._ReloadAllTabs()
    self._CloseAllTabs()

  def _TogglePlugin(self, plugin_name):
    """Toggle plugin status.

    Args:
      plugin_name: Name of the plugin to toggle.
    """
    plugins = self.GetPluginsInfo().Plugins()
    for item in range(len(plugins)):
      if re.search(plugin_name, plugins[item]['name']):
        if plugins[item]['enabled']:
          self.DisablePlugin(plugins[item]['path'])
        else:
          self.EnablePlugin(plugins[item]['path'])

  def _ToggleAndReloadFlashPlugin(self):
    """Toggle flash and reload all tabs."""
    logging.info('In _ToggleAndReloadFlashPlugin')
    for _ in range(10):
      self.AppendTab(pyauto.GURL(self.flash_url3))
    # Disable Flash Plugin
    self._TogglePlugin('Shockwave Flash')
    self._ReloadAllTabs()
    # Enable Flash Plugin
    self._TogglePlugin('Shockwave Flash')
    self._ReloadAllTabs()
    self._CloseAllTabs()

  # Downloads stress functions

  def _LoadDownloadsInMultipleTabs(self):
    """Load Downloads in multiple tabs in the same window."""
    # Open 15 tabs with downloads
    logging.info('In _LoadDownloadsInMultipleTabs')
    for tab_index in range(15):
      # We open an empty tab and then downlad a file from it.
      self.AppendTab(pyauto.GURL(self.empty_url))
      self.NavigateToURL(self.download_url1, 0, tab_index + 1)
      self.AppendTab(pyauto.GURL(self.empty_url))
      self.NavigateToURL(self.download_url2, 0, tab_index + 2)

  def _OpenAndCloseMultipleTabsWithDownloads(self):
    """Download items in multiple tabs."""
    logging.info('In _OpenAndCloseMultipleTabsWithDownloads')
    self._LoadDownloadsInMultipleTabs()
    self._CloseAllTabs()

  def _OpenAndCloseMultipleWindowsWithDownloads(self):
    """Randomly have downloads in multiple windows."""
    logging.info('In _OpenAndCloseMultipleWindowsWithDownloads')
    # Open 15 Windows randomly on both regular and incognito with downloads
    for window_index in range(15):
      tick = round(random.random() * 100)
      if tick % 2 != 0:
        self.NavigateToURL(self.download_url2, 0, 0)
      else:
        self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
        self.AppendTab(pyauto.GURL(self.empty_url), 1)
        self.NavigateToURL(self.download_url2, 1, 1)
    self._CloseAllWindows()

  def _OpenAndCloseMultipleTabsWithMultipleDownloads(self):
    """Download multiple items in multiple tabs."""
    logging.info('In _OpenAndCloseMultipleTabsWithMultipleDownloads')
    self.NavigateToURL(self.empty_url)
    for _ in range(15):
      for file in self.file_list:
        count = 1
        url = self.GetFileURLForPath(
            os.path.join(self.DataDir(), 'downloads', file))
        self.AppendTab(pyauto.GURL(self.empty_url))
        self.NavigateToURL(url, 0, count)
        count = count + 1
      self._CloseAllTabs()

  def _OpenAndCloseMultipleWindowsWithMultipleDownloads(self):
    """Randomly multiple downloads in multiple windows."""
    logging.info('In _OpenAndCloseMultipleWindowsWithMultipleDownloads')
    for _ in range(15):
      for file in self.file_list:
        tick = round(random.random() * 100)
        url = self.GetFileURLForPath(
            os.path.join(self.DataDir(), 'downloads', file))
        if tick % 2!= 0:
          self.NavigateToURL(url, 0, 0)
        else:
          self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
          self.AppendTab(pyauto.GURL(self.empty_url), 1)
          self.NavigateToURL(url, 1, 1)
    self._CloseAllWindows()

  # Back and Forward stress functions

  def _BrowserGoBack(self, window_index):
    """Go back in the browser history.

    Chrome has limitation on going back and can only go back 49 pages.

    Args:
      window_index: the index of the browser window to work on.
    """
    for nback in range(48):  # Go back 48 times.
      tab = self.GetBrowserWindow(window_index).GetTab(0)
      if nback % 4 == 0:   # Bookmark every 5th url when going back.
        self._BookMarkEvery5thURL(window_index)
      tab.GoBack()

  def _BrowserGoForward(self, window_index):
    """Go Forward in the browser history.

    Chrome has limitation on going back and can only go back 49 pages.

    Args:
      window_index: the index of the browser window to work on.
    """
    for nforward in range(48):  # Go back 48 times.
      tab = self.GetBrowserWindow(window_index).GetTab(0)
      if nforward % 4 == 0:  # Bookmark every 5th url when going Forward
           self._BookMarkEvery5thURL(window_index)
      tab.GoForward()

  def _AddToListAndBookmark(self, newname, url):
    """Bookmark the url to bookmarkbar and to he list of bookmarks.

    Args:
      newname: the name of the bookmark.
      url: the url to bookmark.
    """
    bookmarks = self.GetBookmarkModel()
    bar_id = bookmarks.BookmarkBar()['id']
    self.AddBookmarkURL(bar_id, 0, newname, url)
    self.bookmarks_list.append(newname)

  def _RemoveFromListAndBookmarkBar(self, name):
    """Remove the bookmark bor and bookmarks list.

    Args:
      name: the name of bookmark to remove.
    """
    bookmarks = self.GetBookmarkModel()
    node = bookmarks.FindByTitle(name)
    self.RemoveBookmark(node[0]['id'])
    self.bookmarks_list.remove(name)

  def _DuplicateBookmarks(self, name):
    """Find duplicate bookmark in the bookmarks list.

    Args:
      name: name of the bookmark.

    Returns:
      True if it's a duplicate.
    """
    for index in (self.bookmarks_list):
      if index ==  name:
        return True
    return False

  def _BookMarkEvery5thURL(self, window_index):
    """Check for duplicate in list and bookmark current url.
    If its the first time and list is empty add the bookmark.
    If its a duplicate remove the bookmark.
    If its new tab page move over.

    Args:
      window_index: the index of the browser window to work on.
    """
    tab_title = self.GetActiveTabTitle(window_index)  # get the page title
    url = self.GetActiveTabURL(window_index).spec()  # get the page url
    if not self.bookmarks_list:
      self._AddToListAndBookmark(tab_title, url)  # first run bookmark the url
      return
    elif self._DuplicateBookmarks(tab_title):
      self._RemoveFromListAndBookmarkBar(tab_title)
      return
    elif tab_title == 'New Tab':  # new tab page pass over
      return
    else:
      # new bookmark add it to bookmarkbar
      self._AddToListAndBookmark(tab_title, url)
      return

  def _ReadFileAndLoadInNormalAndIncognito(self):
    """Read urls and load them in normal and incognito window.
    We load 96 urls only as we can go back and forth 48 times.
    Uses time to get different urls in normal and incognito window
    The source file is taken from stress folder in /data folder.
    """
    # URL source from stress folder in data folder
    data_file = os.path.join(self.DataDir(), 'pyauto_private', 'stress',
                             'urls_and_titles')
    url_data = self.EvalDataFrom(data_file)
    urls = url_data.keys()
    i = 0
    ticks = int(time.time())  # get the latest time.
    for url in urls:
      if i <= 96 :  # load only 96 urls.
        if ticks % 2 == 0:  # loading in Incognito and Normal window.
          self.NavigateToURL(url)
        else:
          self.NavigateToURL(url, 1, 0)
      else:
        break
      ticks = ticks - 1
      i += 1
    return

  def _StressTestNavigation(self):
    """ This is the method from where various navigations are called.
    First we load the urls then call navigete back and forth in
    incognito window then in normal window.
    """
    self._ReadFileAndLoadInNormalAndIncognito()  # Load the urls.
    self._BrowserGoBack(1)  # Navigate back in incognito window.
    self._BrowserGoForward(1)  # Navigate forward in incognito window
    self._BrowserGoBack(0)  # Navigate back in normal window
    self._BrowserGoForward(0)  # Navigate forward in normal window

  # Preference stress functions

  def _RandomBool(self):
    """For any preferences bool value, it takes True or False value.
    We are generating random True or False value.
    """
    return random.randint(0, 1) == 1

  def _RandomURL(self):
    """Some of preferences take string url, so generating random url here."""
    # Site list
    site_list = ['test1.html', 'test2.html','test3.html','test4.html',
                 'test5.html', 'test7.html', 'test6.html']
    random_site = random.choice(site_list)
    # Returning a url of random site
    return self.GetFileURLForPath(os.path.join(self.DataDir(), random_site))

  def _RandomURLArray(self):
    """Returns a list of 10 random URLs."""
    return [self._RandomURL() for _ in range(10)]

  def _RandomInt(self, max_number):
    """Some of the preferences takes integer value.
    Eg: If there are three options, we generate random
    value for any option.

    Arg:
      max_number: The number of options that a preference has.
    """
    return random.randrange(1, max_number)

  def _RandomDownloadDir(self):
    """Returns a random download directory."""
    return random.choice(['dl_dir1', 'dl_dir2', 'dl_dir3',
                          'dl_dir4', 'dl_dir5'])

  def _SetPref(self):
    """Reads the preferences from file and
       sets the preferences to Chrome.
    """
    raw_dictionary = self.EvalDataFrom(os.path.join(self.DataDir(),
        'pyauto_private', 'stress', 'pref_dict'))
    value_dictionary = {}

    for key, value in raw_dictionary.iteritems():
      if value == 'BOOL':
         value_dictionary[key] = self._RandomBool()
      elif value == 'STRING_URL':
         value_dictionary[key] = self._RandomURL()
      elif value == 'ARRAY_URL':
         value_dictionary[key] = self._RandomURLArray()
      elif value == 'STRING_PATH':
         value_dictionary[key] = self._RandomDownloadDir()
      elif value[0:3] == 'INT':
         # Normally we difine INT datatype with number of options,
         # so parsing number of options and selecting any of them
         # randomly.
         value_dictionary[key] = 1
         max_number = raw_dictionary[key][3:4]
         if not max_number == 1:
           value_dictionary[key]= self._RandomInt(int(max_number))
      self.SetPrefs(getattr(pyauto,key), value_dictionary[key])

    return value_dictionary

  # Crash reporting functions

  def _CrashDumpFolder(self):
    """Get the breakpad folder.

    Returns:
      The full path of the Crash Reports folder.
    """
    breakpad_folder = self.GetBrowserInfo()['properties']['DIR_CRASH_DUMPS']
    self.assertTrue(breakpad_folder, 'Cannot figure crash dir')
    return breakpad_folder

  def _DeleteDumps(self):
    """Delete all the dump files in teh Crash Reports folder."""
    # should be called at the start of stress run
    if os.path.exists(self.breakpad_dir):
      logging.info('xxxxxxxxxxxxxxxINSIDE DELETE DUMPSxxxxxxxxxxxxxxxxx')
      if self.IsMac():
        shutil.rmtree(self.breakpad_dir)
      elif self.IsWin():
        files = os.listdir(self.breakpad_dir)
        for file in files:
          os.remove(file)

    first_crash = os.path.join(os.getcwd(), '1stcrash')
    crashes_dir = os.path.join(os.getcwd(), 'crashes')
    if (os.path.exists(crashes_dir)):
      shutil.rmtree(crashes_dir)
      shutil.rmtree(first_crash)

  def _SymbolicateCrashDmp(self, dmp_file, symbols_dir, output_file):
    """Generate symbolicated crash report.

    Args:
      dmp_file: the dmp file to symbolicate.
      symbols_dir: the directory containing the symbols.
      output_file: the output file.

    Returns:
      Crash report text.
    """
    report = ''
    if self.IsWin():
      windbg_cmd = [
          os.path.join('C:', 'Program Files', 'Debugging Tools for Windows',
                       'windbg.exe'),
          '-Q',
          '-y',
          '\"',
          symbols_dir,
          '\"',
          '-c',
          '\".ecxr;k50;.logclose;q\"',
          '-logo',
          output_file,
          '-z',
          '\"',
          dmp_file,
          '\"']
      subprocess.call(windbg_cmd)
      # Since we are directly writing the info into output_file,
      # we just need to copy that in to report
      report = open(output_file, 'r').read()

    elif self.IsMac():
      crash_report = os.path.join(self.DataDir(), 'pyauto_private', 'stress',
                                  'mac', 'crash_report')
      for i in range(5):  # crash_report doesn't work sometimes. So we retry
        report = test_utils.Shell2(
            '%s -S "%s" "%s"' % (crash_report, symbols_dir, dmp_file))[0]
        if len(report) < 200:
          try_again = 'Try %d. crash_report didn\'t work out. Trying again', i
          logging.info(try_again)
        else:
          break
      open(output_file, 'w').write(report)
    return report

  def _SaveSymbols(self, symbols_dir, dump_dir=' ', multiple_dumps=True):
    """Save the symbolicated files for all crash dumps.

    Args:
      symbols_dir: the directory containing the symbols.
      dump_dir: Path to the directory holding the crash dump files.
      multiple_dumps: True if we are processing multiple dump files,
                      False if we are processing only the first crash.
    """
    if multiple_dumps:
      dump_dir = self.breakpad_dir

    if not os.path.isdir(CRASHES):
      os.makedirs(CRASHES)

    # This will be sent to the method by the caller.
    dmp_files = glob.glob(os.path.join(dump_dir, '*.dmp'))
    for dmp_file in dmp_files:
      dmp_id = os.path.splitext(os.path.basename(dmp_file))[0]
      if multiple_dumps:
        report_folder = CRASHES
      else:
        report_folder = dump_dir
      report_fname = os.path.join(report_folder,
                                  '%s.txt' % (dmp_id))
      report = self._SymbolicateCrashDmp(dmp_file, symbols_dir,
                                         report_fname)
      if report == '':
        logging.info('Crash report is empty.')
      # This is for copying the original dumps.
      if multiple_dumps:
        shutil.copy2(dmp_file, CRASHES)

  def _GetFirstCrashDir(self):
    """Get first crash file in the crash folder.
    Here we create the 1stcrash directory which holds the
    first crash report, which will be attached to the mail.
    """
    breakpad_folder = self.breakpad_dir
    dump_list = glob.glob1(breakpad_folder,'*.dmp')
    dump_list.sort(key=lambda s: os.path.getmtime(os.path.join(
                                                  breakpad_folder, s)))
    first_crash_file = os.path.join(breakpad_folder, dump_list[0])

    if not os.path.isdir('1stcrash'):
      os.makedirs('1stcrash')
    shutil.copy2(first_crash_file, '1stcrash')
    first_crash_dir = os.path.join(os.getcwd(), '1stcrash')
    return first_crash_dir

  def _GetFirstCrashFile(self):
    """Get first crash file in the crash folder."""
    first_crash_dir = os.path.join(os.getcwd(), '1stcrash')
    for each in os.listdir(first_crash_dir):
      if each.endswith('.txt'):
        first_crash_file = each
        return os.path.join(first_crash_dir, first_crash_file)

  def _ProcessOnlyFirstCrash(self):
    """ Process only the first crash report for email."""
    first_dir = self._GetFirstCrashDir()
    self._SaveSymbols(self.symbols_dir, first_dir, False)

  def _GetOSName(self):
    """Returns the OS type we are running this script on."""
    os_name = ''
    if self.IsMac():
      os_number = commands.getoutput('sw_vers -productVersion | cut -c 1-4')
      if os_number == '10.6':
        os_name = 'Snow_Leopard'
      elif os_number == '10.5':
        os_name = 'Leopard'
    elif self.IsWin():
      # TODO: Windows team need to find the way to get OS name
      os_name = 'Windows'
      if platform.version()[0] == '5':
        os_name = os_name + '_XP'
      else:
        os_name = os_name + '_Vista/Win7'
    return os_name

  def _ProcessUploadAndEmailCrashes(self):
    """Upload the crashes found and email the team about this."""
    logging.info('#########INSIDE _ProcessUploadAndEmailCrashes#########')
    try:
      build_version = self.chrome_version
      self._SaveSymbols(self.symbols_dir)
      self._ProcessOnlyFirstCrash()
      file_to_attach =  self._GetFirstCrashFile()
      # removing the crash_txt for now,
      # since we are getting UnicodeDecodeError
      # crash_txt = open(file_to_attach).read()
    except ValueError:
      test_utils.SendMail(self.stress_pref['mailing_address'],
                          self.stress_pref['mailing_address'],
                          "We don't have build version",
                          "BROWSER CRASHED, PLEASE CHECK",
                          self.stress_pref['smtp'])
    # Move crash reports and dumps to server
    os_name = self._GetOSName()
    dest_dir = build_version + '_' + os_name
    if (test_utils.Shell2(self.stress_pref['script'] % (CRASHES, dest_dir))):
      logging.info('Copy Complete')
    upload_dir= self.stress_pref['upload_dir'] + dest_dir
    num_crashes =  '\n \n Number of Crashes :' + \
                   str(len(glob.glob1(self.breakpad_dir, '*.dmp')))
    mail_content =  '\n\n Crash Report URL :' + upload_dir + '\n' + \
                    num_crashes + '\n\n' # + crash_txt
    mail_subject = 'Stress Results :' + os_name + '_' + build_version
    # Sending mail with first crash report, # of crashes, location of upload
    test_utils.SendMail(self.stress_pref['mailing_address'],
                        self.stress_pref['mailing_address'],
                        mail_subject, mail_content,
                        self.stress_pref['smtp'], file_to_attach)

  def _ReportCrashIfAny(self):
    """Check for browser crashes and report."""
    if os.path.isdir(self.breakpad_dir):
      listOfDumps = glob.glob(os.path.join(self.breakpad_dir, '*.dmp'))
      if len(listOfDumps) > 0:
        logging.info('========== INSIDE REPORT CRASH++++++++++++++')
        # inform a method to process the dumps
        self._ProcessUploadAndEmailCrashes()

  # Test functions

  def _PrefStress(self):
    """Stress preferences."""
    default_prefs = self.GetPrefsInfo()
    pref_dictionary = self._SetPref()
    for key, value in pref_dictionary.iteritems():
      self.assertEqual(value, self.GetPrefsInfo().Prefs(
                       getattr(pyauto, key)))

    for key, value in pref_dictionary.iteritems():
      self.SetPrefs(getattr(pyauto, key),
                    default_prefs.Prefs(getattr(pyauto, key)))

  def _NavigationStress(self):
    """Run back and forward stress in normal and incognito window."""
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self._StressTestNavigation()

  def _DownloadStress(self):
    """Run all the Download stress test."""
    org_download_dir = self.GetDownloadDirectory().value()
    new_dl_dir = os.path.join(org_download_dir, 'My+Downloads Folder')
    os.path.exists(new_dl_dir) and shutil.rmtree(new_dl_dir)
    os.makedirs(new_dl_dir)
    self.SetPrefs(pyauto.kDownloadDefaultDirectory, new_dl_dir)
    self._OpenAndCloseMultipleTabsWithDownloads()
    self._OpenAndCloseMultipleWindowsWithDownloads()
    self._OpenAndCloseMultipleTabsWithMultipleDownloads()
    self._OpenAndCloseMultipleWindowsWithMultipleDownloads()
    pyauto_utils.RemovePath(new_dl_dir)  # cleanup
    self.SetPrefs(pyauto.kDownloadDefaultDirectory, org_download_dir)

  def _PluginStress(self):
    """Run all the plugin stress tests."""
    self._OpenAndCloseMultipleTabsWithFlash()
    self._OpenAndCloseMultipleWindowsWithFlash()
    self._OpenAndCloseMultipleTabsWithMultiplePlugins()
    self._OpenAndCloseMultipleWindowsWithMultiplePlugins()
    self._KillAndReloadRenderersWithFlash()
    self._ToggleAndReloadFlashPlugin()

  def testStress(self):
    """Run all the stress tests for 24 hrs."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      logging.info('This is not a branded build, so stopping the stress')
      return 1
    self._DownloadSymbols()
    run_number = 1
    start_time = time.time()
    while True:
      logging.info('run %d...' % run_number)
      run_number = run_number + 1
      if (time.time() - start_time) >= 24*60*60:
        logging.info('Its been 24hrs, so we break now.')
        break
      try:
        methods = [self._NavigationStress, self._DownloadStress,
                   self._PluginStress, self._PrefStress]
        random.shuffle(methods)
        for method in methods:
          method()
          logging.info('Method %s done' % method)
      except KeyboardInterrupt:
        logging.info('----------We got a KeyboardInterrupt-----------')
      except Exception, error:
        logging.info('-------------There was an ERROR---------------')
        logging.info(error)

      # Crash Reporting
      self._ReportCrashIfAny()
      self._DeleteDumps()

    if self.IsMac():
      zombie = 'ps -el | grep Chrom | grep -v grep | grep Z | wc -l'
      zombie_count = int(commands.getoutput(zombie))
      if zombie_count > 0:
        logging.info('WE HAVE ZOMBIES = %d' % zombie_count)


if __name__ == '__main__':
  pyauto_functional.Main()

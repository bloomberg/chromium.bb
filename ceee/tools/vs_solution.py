#!python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''This script implements a class to handle setup of Visual Studio solution
and DTE objects.
'''
try:
  import pythoncom
  import win32com.client
except ImportError:
  print "This script requires Python 2.6 with PyWin32 installed."
  raise


class VsSolution(object):
  def __init__(self, solution_file):
    self._solution = win32com.client.GetObject(solution_file)
    self._dte = self._InitDte()

  def _InitDte(self):
    '''Sometimes invoking solution.DTE will fail with an exception during
    Visual Studio initialization. To work around this, we try a couple of
    times with an intervening sleep to give VS time to settle.'''
    # Attempt ten times under a try block.
    for i in range(0, 10):
      try:
        return self._solution.DTE
      except pythoncom.com_error, e:
        # Sleep for 2 seconds
        print 'Exception from solution.DTE "%s", retrying: ' % str(e)
        time.sleep(2.0)

  def GetDte(self):
    return self._dte

  def GetSolution(self):
    return self._solution

  def EnsureVisible(self):
    dte = self._dte
    # Make sure the UI is visible.
    dte.MainWindow.Visible = True

    # Get the output window, disable its autohide and give it focus.
    try:
      self._output = dte.Windows['Output']
      self._saved_autohides = self._output.AutoHides
      self._output.AutoHides = False
      self._output.SetFocus()
    except TypeError, e:
      print 'Error forcing Visual Studio Output Window Visible'

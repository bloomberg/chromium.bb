# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os


class ChromotingMixIn(object):
  """MixIn for PyUITest that adds Chromoting-specific methods.

  Prepend it as a base class of a test to enable Chromoting functionality.
  This is a separate class from PyUITest to avoid namespace collisions.

  Example usage:
    class ChromotingExample(chromoting.ChromotingMixIn, pyauto.PyUITest):
      def testShare(self):
        app = self.InstallApp(self.GetIT2MeAppPath())
        self.LaunchApp(app)
        self.Authenticate()
        self.SetHostMode()
        self.assertTrue(self.Share())
  """

  def _ExecuteJavascript(self, command, tab_index, windex):
    """Helper that returns immediately after running a Javascript command."""
    return self.ExecuteJavascript(
        '%s; window.domAutomationController.send("done");' % command,
        tab_index, windex)

  def _WaitForJavascriptCondition(self, condition, tab_index, windex):
    """Waits until the Javascript condition is true.

    This is different from a naive self.WaitUntil(lambda: self.GetDOMValue())
    because it uses Javascript to check the condition instead of Python.
    """
    return self.WaitUntil(lambda: self.GetDOMValue(
        '(%s) ? "1" : ""' % condition, tab_index, windex))

  def _ExecuteAndWaitForMode(self, command, mode, tab_index, windex):
    self.assertTrue(self._ExecuteJavascript(command, tab_index, windex),
                    'Javascript command did not return anything.')
    return self._WaitForJavascriptCondition(
        'remoting.currentMode == remoting.AppMode.%s' % mode,
        tab_index, windex)

  def _ExecuteAndWaitForMajorMode(self, command, mode, tab_index, windex):
    self.assertTrue(self._ExecuteJavascript(command, tab_index, windex),
                    'Javascript command did not return anything.')
    return self._WaitForJavascriptCondition(
        'remoting.getMajorMode() == remoting.AppMode.%s' % mode,
        tab_index, windex)

  def GetIT2MeAppPath(self):
    """Returns the path to the IT2Me App.

    Expects the IT2Me webapp to be in the same place as the pyautolib binaries.
    """
    return os.path.join(self.BrowserPath(), 'remoting', 'it2me.webapp')

  def Authenticate(self, email=None, password=None, otp=None,
                   tab_index=1, windex=0):
    """Logs a user in for Chromoting and accepts permissions for the app.

    PyAuto tests start with a clean profile, so Chromoting tests should call
    this for every run after launching the app. If email or password is omitted,
    the user can type it into the browser window manually.

    Raises:
      AssertionError if the authentication flow changes or
          the credentials are incorrect.
    """
    self.assertTrue(
        self._WaitForJavascriptCondition('window.remoting && remoting.oauth2',
                                         tab_index, windex),
        msg='Timed out while waiting for remoting app to finish loading.')
    self._ExecuteJavascript('remoting.oauth2.doAuthRedirect();',
                            tab_index, windex)
    self.assertTrue(
        self._WaitForJavascriptCondition('document.getElementById("signIn")',
                                         tab_index, windex),
        msg='Unable to redirect for authentication.')

    if email:
      self._ExecuteJavascript('document.getElementById("Email").value = "%s";'
                              'document.getElementById("Passwd").focus();'
                              % email, tab_index, windex)

    if password:
      self._ExecuteJavascript('document.getElementById("Passwd").value = "%s";'
                              'document.getElementById("signIn").click();'
                              % password, tab_index, windex)

    if otp:
      self.assertTrue(
          self._WaitForJavascriptCondition(
              'document.getElementById("smsVerifyPin")',
              tab_index, windex),
          msg='Invalid username or password.')
      self._ExecuteJavascript('document.getElementById("smsUserPin").value = '
                              '"%s";'
                              'document.getElementById("smsVerifyPin").click();'
                              % otp, tab_index, windex)

    # Approve access.
    self.assertTrue(
        self._WaitForJavascriptCondition(
            'document.getElementById("submit_approve_access")',
            tab_index, windex),
        msg='Authentication failed. The username, password, or otp is invalid.')
    self._ExecuteJavascript(
        'document.getElementById("submit_approve_access").click();',
        tab_index, windex)

    # Wait for some things to be ready.
    self.assertTrue(
        self._WaitForJavascriptCondition(
            'window.remoting && remoting.oauth2 && '
            'remoting.oauth2.isAuthenticated()',
            tab_index, windex),
        msg='OAuth2 authentication failed.')
    self.assertTrue(
        self._WaitForJavascriptCondition(
            'window.localStorage.getItem("remoting-email")',
            tab_index, windex),
        msg='Chromoting app did not reload after authentication.')

  def SetHostMode(self, tab_index=1, windex=0):
    """Sets the Chromoting app to host mode, perfect for sharing!

    Returns:
      True on success; False otherwise.
    """
    return self._ExecuteAndWaitForMajorMode(
        'remoting.setAppMode(remoting.AppMode.HOST);',
        'HOST', tab_index, windex)

  def SetClientMode(self, tab_index=1, windex=0):
    """Sets the Chromoting app to client mode, perfect for connecting to a host!

    Returns:
      True on success; False otherwise.
    """
    return self._ExecuteAndWaitForMajorMode(
        'remoting.setAppMode(remoting.AppMode.CLIENT);',
        'CLIENT', tab_index, windex)

  def Share(self, tab_index=1, windex=0):
    """Generates an access code and waits for incoming connections.

    Returns:
      The access code on success; None otherwise.
    """
    self._ExecuteAndWaitForMode(
        'remoting.tryShare();',
        'HOST_WAITING_FOR_CONNECTION', tab_index, windex)
    return self.GetDOMValue(
        'document.getElementById("access-code-display").innerText',
        tab_index, windex)

  def Connect(self, access_code, tab_index=1, windex=0):
    """Connects to a Chromoting host and starts the session.

    Returns:
      True on success; False otherwise.
    """
    return self._ExecuteAndWaitForMode(
        'document.getElementById("access-code-entry").value = "%s";'
        'remoting.tryConnect();' % access_code,
        'IN_SESSION', tab_index, windex)

  def CancelShare(self, tab_index=1, windex=0):
    """Stops sharing the desktop on the host side.

    Returns:
      True on success; False otherwise.
    """
    return self._ExecuteAndWaitForMode(
        'remoting.cancelShare();',
        'HOST_SHARE_FINISHED', tab_index, windex)

  def Disconnect(self, tab_index=1, windex=0):
    """Disconnects from the Chromoting session on the client side.

    Returns:
      True on success; False otherwise.
    """
    return self._ExecuteAndWaitForMode(
        'remoting.disconnect();',
        'CLIENT_SESSION_FINISHED', tab_index, windex)

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Includes different methods to drive chromoting UI."""

import os
import subprocess
import sys
import time

from pyauto_errors import JSONInterfaceError


class ChromotingMixIn(object):
  """MixIn for PyUITest that adds Chromoting-specific methods.

  Prepend it as a base class of a test to enable Chromoting functionality.
  This is a separate class from PyUITest to avoid namespace collisions.

  Example usage:
    class ChromotingExample(chromoting.ChromotingMixIn, pyauto.PyUITest):
      def testShare(self):
        app = self.InstallApp(self.GetWebappPath())
        self.LaunchApp(app)
        self.Authenticate()
        self.assertTrue(self.Share())
  """

  def _ExecuteJavascript(self, command, tab_index, windex):
    """Helper that returns immediately after running a Javascript command.
    """
    try:
      self.ExecuteJavascript(
          '%s; window.domAutomationController.send("done");' % command,
          tab_index, windex)
      return True
    except JSONInterfaceError:
      print '_ExecuteJavascript threw JSONInterfaceError'
      return False

  def _WaitForJavascriptCondition(self, condition, tab_index, windex,
                                  timeout=-1):
    """Waits until the Javascript condition is true.

    This is different from a naive self.WaitUntil(lambda: self.GetDOMValue())
    because it uses Javascript to check the condition instead of Python.

    Returns: True if condition is satisfied or otherwise False.
    """
    try:
      return self.WaitUntil(lambda: self.GetDOMValue(
          '(%s) ? "1" : ""' % condition, tab_index, windex), timeout)
    except JSONInterfaceError:
      print '_WaitForJavascriptCondition threw JSONInterfaceError'
      return False

  def _ExecuteAndWaitForMode(self, command, mode, tab_index, windex):
    """ Executes JavaScript and wait for remoting app mode equal to
    the given mode.

    Returns: True if condition is satisfied or otherwise False.
    """
    if not self._ExecuteJavascript(command, tab_index, windex):
      return False
    return self._WaitForJavascriptCondition(
        'remoting.currentMode == remoting.AppMode.%s' % mode,
        tab_index, windex)

  def _ExecuteAndWaitForMajorMode(self, command, mode, tab_index, windex):
    """ Executes JavaScript and wait for remoting app major mode equal to
    the given mode.

    Returns: True if condition is satisfied or otherwise False.
    """
    if not self._ExecuteJavascript(command, tab_index, windex):
      return False
    return self._WaitForJavascriptCondition(
        'remoting.getMajorMode() == remoting.AppMode.%s' % mode,
        tab_index, windex)

  def GetWebappPath(self):
    """Returns the path to the webapp.

    Expects the webapp to be in the same place as the pyautolib binaries.
    """
    return os.path.join(self.BrowserPath(), 'remoting', 'remoting.webapp')

  def _GetHelperRunner(self):
    """Returns the python binary name that runs chromoting_helper.py."""
    if sys.platform.startswith('win'):
      return 'python'
    else:
      return 'suid-python'

  def _GetHelper(self):
    """Get chromoting_helper.py."""
    return os.path.join(os.path.dirname(__file__), 'chromoting_helper.py')

  def InstallHostDaemon(self):
    """Installs the host daemon."""
    subprocess.call([self._GetHelperRunner(), self._GetHelper(),
                     'install', self.BrowserPath()])

  def UninstallHostDaemon(self):
    """Uninstalls the host daemon."""
    subprocess.call([self._GetHelperRunner(), self._GetHelper(),
                     'uninstall', self.BrowserPath()])

  def ContinueAuth(self, tab_index=1, windex=0):
    """Starts authentication."""
    self.assertTrue(
        self._WaitForJavascriptCondition('window.remoting && remoting.oauth2',
                                         tab_index, windex),
        msg='Timed out while waiting for remoting app to finish loading.')
    self._ExecuteJavascript('remoting.oauth2.doAuthRedirect();',
                            tab_index, windex)

  def SignIn(self, email=None, password=None, otp=None,
                   tab_index=1, windex=0):
    """Logs a user in.

    PyAuto tests start with a clean profile, so Chromoting tests should call
    this for every run after launching the app. If email or password is
    omitted, the user can type it into the browser window manually.
    """
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
      self._ExecuteJavascript(
          'document.getElementById("smsUserPin").value = "%s";'
          'document.getElementById("smsVerifyPin").click();' % otp,
          tab_index, windex)

    # If the account adder screen appears, then skip it.
    self.assertTrue(
        self._WaitForJavascriptCondition(
            'document.getElementById("skip") || '
            'document.getElementById("submit_approve_access")',
            tab_index, windex),
        msg='No "skip adding account" or "approve access" link.')
    self._ExecuteJavascript(
        'if (document.getElementById("skip")) '
        '{ document.getElementById("skip").click(); }',
        tab_index, windex)

  def AllowAccess(self, tab_index=1, windex=0):
    """Allows access to chromoting webapp."""
    # Approve access.
    self.assertTrue(
        self._WaitForJavascriptCondition(
            'document.getElementById("submit_approve_access")',
            tab_index, windex),
        msg='Did not go to permission page.')
    self._ExecuteJavascript(
        'document.getElementById("submit_approve_access").click();',
        tab_index, windex)

    # Wait for some things to be ready.
    self.assertTrue(
        self._WaitForJavascriptCondition(
            'window.remoting && remoting.oauth2 && ' \
            'remoting.oauth2.isAuthenticated()',
            tab_index, windex),
        msg='OAuth2 authentication failed.')
    self.assertTrue(
        self._WaitForJavascriptCondition(
            'window.localStorage.getItem("remoting-email")',
            tab_index, windex),
        msg='Chromoting app did not reload after authentication.')

  def DenyAccess(self, tab_index=1, windex=0):
    """Deny and then allow access to chromoting webapp."""
    self.assertTrue(
        self._WaitForJavascriptCondition(
            'document.getElementById("submit_deny_access")',
            tab_index, windex),
        msg='Did not go to permission page.')
    self._ExecuteJavascript(
        'document.getElementById("submit_deny_access").click();',
        tab_index, windex)

  def SignOut(self, tab_index=1, windex=0):
    """Signs out from chromoting and signs back in."""
    self._ExecuteAndWaitForMode(
        'document.getElementById("sign-out").click();',
        'UNAUTHENTICATED', tab_index, windex)

  def Authenticate(self, tab_index=1, windex=0):
    """Finishes authentication flow for user."""
    self.ContinueAuth(tab_index, windex)
    account = self.GetPrivateInfo()['test_chromoting_account']
    self.host.SignIn(account['username'], account['password'], None,
                    tab_index, windex)
    self.host.AllowAccess(tab_index, windex)

  def StartMe2Me(self, tab_index=1, windex=0):
    """Starts Me2Me. """
    self._ExecuteJavascript(
        'document.getElementById("get-started-me2me").click();',
        tab_index, windex)
    self.assertTrue(
        self._WaitForJavascriptCondition(
            'document.getElementById("me2me-content").hidden == false',
            tab_index, windex),
        msg='No me2me content')

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

  def CancelShare(self, tab_index=1, windex=0):
    """Stops sharing the desktop on the host side."""
    self.assertTrue(
        self._ExecuteAndWaitForMode(
            'remoting.cancelShare();',
            'HOST_SHARE_FINISHED', tab_index, windex),
        msg='Stopping sharing from the host side failed')

  def CleanupHostList(self, tab_index=1, windex=0):
    """Removes hosts due to failure on previous stop-daemon"""
    self.EnableConnectionsInstalled()
    this_host_name = self.GetDOMValue(
        'document.getElementById("this-host-name").textContent',
        tab_index, windex)
    if this_host_name.endswith(' (offline)'):
      this_host_name = this_host_name[:-10]
    self.DisableConnections()

    total_hosts = self.GetDOMValue(
        'document.getElementById("host-list").childNodes.length',
        tab_index, windex)

    # Start from the end while deleting bogus hosts
    index = total_hosts
    while index > 0:
      index -= 1
      try:
        hostname = self.GetDOMValue(
            'document.getElementById("host-list")'
            '.childNodes[%s].textContent' % index,
            tab_index, windex)
        if hostname == this_host_name or \
            hostname == this_host_name + ' (offline)':
          self._ExecuteJavascript(
              'document.getElementById("host-list")'
              '.childNodes[%s].childNodes[3].click()' % index,
              tab_index, windex)
          self._ExecuteJavascript(
              'document.getElementById("confirm-host-delete").click()',
              tab_index, windex)
      except JSONInterfaceError:
        print 'Ignore the error on deleting host'

    if self._WaitForJavascriptCondition(
            'document.getElementById("this-host-connect")'
            '.getAttribute("data-daemon-state") == "enabled"',
            tab_index, windex, 1):
      self.DisableConnections()

  def EnableConnectionsInstalled(self, pin_exercise=False,
                                 tab_index=1, windex=0):
    """Enables the remote connections on the host side."""
    if sys.platform.startswith('darwin'):
      subprocess.call([self._GetHelperRunner(), self._GetHelper(), 'enable'])

    self.assertTrue(
        self._ExecuteAndWaitForMode(
            'document.getElementById("start-daemon").click();',
            'HOST_SETUP_ASK_PIN', tab_index, windex),
        msg='Cannot start host setup')
    self.assertTrue(
        self._WaitForJavascriptCondition(
            'document.getElementById("ask-pin-form").hidden == false',
            tab_index, windex),
        msg='No ask pin dialog')

    if pin_exercise:
      # Cancels the pin prompt
      self._ExecuteJavascript(
          'document.getElementById("daemon-pin-cancel").click();',
          tab_index, windex)

      # Enables again
      self.assertTrue(
          self._ExecuteAndWaitForMode(
              'document.getElementById("start-daemon").click();',
              'HOST_SETUP_ASK_PIN', tab_index, windex),
          msg='Cannot start host setup')

      # Click ok without typing in pins
      self._ExecuteJavascript(
          'document.getElementById("daemon-pin-ok").click();',
          tab_index, windex)
      self.assertTrue(
          self._WaitForJavascriptCondition(
              'document.getElementById("daemon-pin-error-message")',
              tab_index, windex),
          msg='No pin error message')

      # Mis-matching pins
      self._ExecuteJavascript(
          'document.getElementById("daemon-pin-entry").value = "111111";',
          tab_index, windex)
      self._ExecuteJavascript(
          'document.getElementById("daemon-pin-confirm").value = "123456";',
          tab_index, windex)
      self.assertTrue(
          self._WaitForJavascriptCondition(
              'document.getElementById("daemon-pin-error-message")',
              tab_index, windex),
          msg='No pin error message')

    # Types in correct pins
    self._ExecuteJavascript(
        'document.getElementById("daemon-pin-entry").value = "111111";',
        tab_index, windex)
    self._ExecuteJavascript(
        'document.getElementById("daemon-pin-confirm").value = "111111";',
        tab_index, windex)
    self.assertTrue(
        self._ExecuteAndWaitForMode(
            'document.getElementById("daemon-pin-ok").click();',
            'HOST_SETUP_PROCESSING', tab_index, windex),
        msg='Host setup was not started')

    # Handles preference panes
    self.assertTrue(
        self._WaitForJavascriptCondition(
            'remoting.currentMode == remoting.AppMode.HOST_SETUP_DONE',
            tab_index, windex),
        msg='Host setup was not done')

    # Dismisses the host config done dialog
    self.assertTrue(
        self._WaitForJavascriptCondition(
            'document.getElementById("host-setup-dialog")'
            '.childNodes[5].hidden == false',
            tab_index, windex),
        msg='No host setup done dialog')
    self.assertTrue(
        self._ExecuteAndWaitForMode(
            'document.getElementById("host-config-done-dismiss").click();',
            'HOME', tab_index, windex),
        msg='Failed to dismiss host setup confirmation dialog')

  def EnableConnectionsUninstalledAndCancel(self, tab_index=1, windex=0):
    """Enables remote connections while host is not installed yet."""
    self.assertTrue(
        self._ExecuteAndWaitForMode(
            'document.getElementById("start-daemon").click();',
            'HOST_SETUP_INSTALL', tab_index, windex),
        msg='Cannot start host install')
    self.assertTrue(
        self._ExecuteAndWaitForMode(
            'document.getElementById("host-config-install-dismiss").click();',
            'HOME', tab_index, windex),
        msg='Failed to dismiss host install dialog')

  def DisableConnections(self, tab_index=1, windex=0):
    """Disables the remote connections on the host side."""
    if sys.platform.startswith('darwin'):
      subprocess.call([self._GetHelperRunner(), self._GetHelper(), 'disable'])

    # Re-try to make disabling connection more stable
    for _ in range (1, 4):
      self._ExecuteJavascript(
          'document.getElementById("stop-daemon").click();',
          tab_index, windex)

      # Immediately waiting for host-setup-dialog hidden sometimes times out
      # even though visually it is hidden. Add some sleep here
      time.sleep(2)

      if self._WaitForJavascriptCondition(
          'document.getElementById("host-setup-dialog")'
          '.childNodes[3].hidden == true',
          tab_index, windex, 1):
        break;

    self.assertTrue(
        self._ExecuteAndWaitForMode(
            'document.getElementById("host-config-done-dismiss").click();',
            'HOME', tab_index, windex),
        msg='Failed to dismiss host setup confirmation dialog')

  def Connect(self, access_code, tab_index=1, windex=0):
    """Connects to a Chromoting host and starts the session."""
    self.assertTrue(
        self._ExecuteAndWaitForMode(
            'document.getElementById("access-code-entry").value = "%s";'
            'remoting.connectIt2Me();' % access_code,
            'IN_SESSION', tab_index, windex),
        msg='Cannot connect it2me session')

  def ChangePin(self, pin='222222', tab_index=1, windex=0):
    """Changes pin for enabled host."""
    if sys.platform.startswith('darwin'):
      subprocess.call([self._GetHelperRunner(), self._GetHelper(), 'changepin'])

    self.assertTrue(
        self._ExecuteAndWaitForMode(
            'document.getElementById("change-daemon-pin").click();',
            'HOST_SETUP_ASK_PIN', tab_index, windex),
        msg='Cannot change daemon pin')
    self.assertTrue(
        self._WaitForJavascriptCondition(
            'document.getElementById("ask-pin-form").hidden == false',
            tab_index, windex),
        msg='No ask pin dialog')

    self._ExecuteJavascript(
        'document.getElementById("daemon-pin-entry").value = "' + pin + '";',
        tab_index, windex)
    self._ExecuteJavascript(
        'document.getElementById("daemon-pin-confirm").value = "' +
        pin + '";', tab_index, windex)
    self.assertTrue(
        self._ExecuteAndWaitForMode(
            'document.getElementById("daemon-pin-ok").click();',
            'HOST_SETUP_PROCESSING', tab_index, windex),
        msg='Host setup was not started')

    # Handles preference panes
    self.assertTrue(
        self._WaitForJavascriptCondition(
            'remoting.currentMode == remoting.AppMode.HOST_SETUP_DONE',
            tab_index, windex),
        msg='Host setup was not done')

    # Dismisses the host config done dialog
    self.assertTrue(
        self._WaitForJavascriptCondition(
            'document.getElementById("host-setup-dialog")'
            '.childNodes[5].hidden == false',
            tab_index, windex),
        msg='No host setup done dialog')
    self.assertTrue(
        self._ExecuteAndWaitForMode(
            'document.getElementById("host-config-done-dismiss").click();',
            'HOME', tab_index, windex),
        msg='Failed to dismiss host setup confirmation dialog')

  def ChangeName(self, new_name='Changed', tab_index=1, windex=0):
    """Changes the host name."""
    self._ExecuteJavascript(
        'document.getElementById("this-host-rename").click();',
        tab_index, windex)
    self._ExecuteJavascript(
        'document.getElementById("this-host-name").childNodes[0].value = "' +
        new_name + '";', tab_index, windex)
    self._ExecuteJavascript(
        'document.getElementById("this-host-rename").click();',
        tab_index, windex)

  def ConnectMe2Me(self, pin='111111', mode='IN_SESSION',
                   tab_index=1, windex=0):
    """Connects to a Chromoting host and starts the session."""

    # There is delay from the enabling remote connections to the host
    # showing up in the host list. We need to reload the web app to get
    # the host to show up. We will repeat this a few times to make sure
    # eventually host appears.
    for _ in range(1, 13):
      self._ExecuteJavascript(
          'window.location.reload();',
          tab_index, windex)

      # pyauto _GetResultFromJSONRequest throws JSONInterfaceError after
      # 45 seconds if ExecuteJavascript is called right after reload.
      # Waiting 2s here can avoid this. So instead of getting the error and
      # wait 45s, we wait 2s here. If the error still happens, the following
      # retry will handle that.
      time.sleep(2)

      # If this-host-connect is still not enabled, let's retry one more time.
      this_host_connect_enabled = False
      for _ in range(1, 3):
        daemon_state_enabled = self._WaitForJavascriptCondition(
            'document.getElementById("this-host-connect")'
            '.getAttribute("data-daemon-state") == "enabled"',
            tab_index, windex, 1)
        host_online = self._WaitForJavascriptCondition(
            'document.getElementById("this-host-name")'
            '.textContent.toString().indexOf("offline") == -1',
            tab_index, windex, 1)
        this_host_connect_enabled = daemon_state_enabled and host_online
        if this_host_connect_enabled:
          break
      if this_host_connect_enabled:
        break;

    # Clicking this-host-connect does work right after this-host-connect
    # is enabled. Need to retry.
    for _ in range(1, 4):
      self._ExecuteJavascript(
          'document.getElementById("this-host-connect").click();',
          tab_index, windex)

      # pyauto _GetResultFromJSONRequest throws JSONInterfaceError after
      # a long time out if WaitUntil is called right after click.
      # Waiting 2s here can avoid this.
      time.sleep(2)

      # If cannot detect that pin-form appears, retry one more time.
      pin_form_exposed = False
      for _ in range(1, 3):
        pin_form_exposed = self._WaitForJavascriptCondition(
            'document.getElementById("client-dialog")'
            '.childNodes[9].hidden == false',
            tab_index, windex, 1)
        if pin_form_exposed:
          break

      if pin_form_exposed:
        break

      # Dismiss connect failure dialog before retry
      if self._WaitForJavascriptCondition(
          'document.getElementById("client-dialog")'
          '.childNodes[25].hidden == false',
          tab_index, windex, 1):
        self._ExecuteJavascript(
            'document.getElementById("client-finished-me2me-button")'
            '.click();',
            tab_index, windex)

    self._ExecuteJavascript(
        'document.getElementById("pin-entry").value = "' + pin + '";',
        tab_index, windex)
    self.assertTrue(
        self._ExecuteAndWaitForMode(
            'document.getElementById("pin-form").childNodes[5].click();',
            mode, tab_index, windex),
        msg='Session was not started')

  def Disconnect(self, tab_index=1, windex=0):
    """Disconnects from the Chromoting it2me session on the client side."""
    self.assertTrue(
        self._ExecuteAndWaitForMode(
            'remoting.disconnect();',
            'CLIENT_SESSION_FINISHED_IT2ME', tab_index, windex),
        msg='Disconnecting it2me session from the client side failed')

  def DisconnectMe2Me(self, confirmation=True, tab_index=1, windex=0):
    """Disconnects from the Chromoting me2me session on the client side."""
    self.assertTrue(
        self._ExecuteAndWaitForMode(
            'remoting.disconnect();',
            'CLIENT_SESSION_FINISHED_ME2ME', tab_index, windex),
        msg='Disconnecting me2me session from the client side failed')

    if confirmation:
      self.assertTrue(
          self._ExecuteAndWaitForMode(
              'document.getElementById("client-finished-me2me-button")'
              '.click();', 'HOME', tab_index, windex),
          msg='Failed to dismiss session finished dialog')

  def ReconnectMe2Me(self, pin='111111', tab_index=1, windex=0):
    """Reconnects the me2me session."""
    self._ExecuteJavascript(
        'document.getElementById("client-reconnect-button").click();',
        tab_index, windex)

    # pyauto _GetResultFromJSONRequest throws JSONInterfaceError after
    # a long time out if WaitUntil is called right after click.
    time.sleep(2)

    # If cannot detect that pin-form appears, retry one more time.
    for _ in range(1, 3):
      pin_form_exposed = self._WaitForJavascriptCondition(
          'document.getElementById("client-dialog")'
          '.childNodes[9].hidden == false',
          tab_index, windex, 1)
      if pin_form_exposed:
        break

    self._ExecuteJavascript(
        'document.getElementById("pin-entry").value = "' + pin + '";',
        tab_index, windex)
    self.assertTrue(
        self._ExecuteAndWaitForMode(
            'document.getElementById("pin-form").childNodes[5].click();',
            'IN_SESSION', tab_index, windex),
        msg='Session was not started when reconnecting')

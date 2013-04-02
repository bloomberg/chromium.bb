# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""TestEnvironment classes.

These classes abstract away the various setups needed to run the WebDriver java
tests in various environments.
"""

import os
import sys

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))

sys.path.insert(0, os.path.join(_THIS_DIR, os.pardir, 'pylib'))

from common import util
from continuous_archive import CHROME_26_REVISION

if util.IsLinux():
  sys.path.insert(0, os.path.join(_THIS_DIR, os.pardir, os.pardir, os.pardir,
                                  'build', 'android'))
  from pylib import android_commands
  from pylib import forwarder
  from pylib import valgrind_tools

ANDROID_TEST_HTTP_PORT = 2311
ANDROID_TEST_HTTPS_PORT = 2411


_REVISION_NEGATIVE_FILTER = {}
_REVISION_NEGATIVE_FILTER['HEAD'] = [
    # https://code.google.com/p/chromedriver/issues/detail?id=283
    'ExecutingAsyncJavascriptTest#'
    'shouldNotTimeoutIfScriptCallsbackInsideAZeroTimeout',
]
_REVISION_NEGATIVE_FILTER[CHROME_26_REVISION] = (
    _REVISION_NEGATIVE_FILTER['HEAD'] + [
        'UploadTest#testFileUploading',
        'AlertsTest',
    ]
)

_OS_NEGATIVE_FILTER = {}
_OS_NEGATIVE_FILTER['win'] = [
    # https://code.google.com/p/chromedriver/issues/detail?id=282
    'PageLoadingTest#'
    'testShouldNotHangIfDocumentOpenCallIsNeverFollowedByDocumentCloseCall',
]
_OS_NEGATIVE_FILTER['linux'] = [
    # https://code.google.com/p/chromedriver/issues/detail?id=284
    'TypingTest#testArrowKeysAndPageUpAndDown',
    'TypingTest#testHomeAndEndAndPageUpAndPageDownKeys',
    'TypingTest#testNumberpadKeys',
]
_OS_NEGATIVE_FILTER['mac'] = []
_OS_NEGATIVE_FILTER['android'] = [
    'BasicMouseInterfaceTest#testDoubleClick',
    'BasicMouseInterfaceTest#testDoubleClickThenGet',
    'BasicMouseInterfaceTest#testDragAndDrop',
    'BasicMouseInterfaceTest#testDraggingElementWithMouseFiresEvents',
    'BasicMouseInterfaceTest#testDraggingElementWithMouseMovesItToAnotherList',
    'BasicMouseInterfaceTest#testMoveAndClick',
    'ClickTest#testCanClickOnAnElementWithTopSetToANegativeNumber',
    'ClickTest#testShouldOnlyFollowHrefOnce',
    'CorrectEventFiringTest#testSendingKeysToAFocusedElementShouldNotBlurThatElement',
    'CorrectEventFiringTest#testSendingKeysToAnElementShouldCauseTheFocusEventToFire',
    'CorrectEventFiringTest#testSendingKeysToAnotherElementShouldCauseTheBlurEventToFire',
    'CorrectEventFiringTest#testShouldEmitClickEventWhenClickingOnATextInputElement',
    'ElementAttributeTest#testCanRetrieveTheCurrentValueOfATextFormField_emailInput',
    'ElementAttributeTest#testCanRetrieveTheCurrentValueOfATextFormField_textArea',
    'ElementAttributeTest#testCanRetrieveTheCurrentValueOfATextFormField_textInput',
    'ExecutingAsyncJavascriptTest#shouldBeAbleToExecuteAsynchronousScripts',
    'ExecutingAsyncJavascriptTest#shouldNotTimeoutIfScriptCallsbackInsideAZeroTimeout',
    'FrameSwitchingTest#testClosingTheFinalBrowserWindowShouldNotCauseAnExceptionToBeThrown',
    'I18nTest#testEnteringHebrewTextFromLeftToRight',
    'I18nTest#testEnteringHebrewTextFromRightToLeft',
    'JavascriptEnabledDriverTest#testIssue80ClickShouldGenerateClickEvent',
    'JavascriptEnabledDriverTest#testShouldFireOnChangeEventWhenSettingAnElementsValue',
    'SelectElementTest#shouldAllowOptionsToBeDeselectedByIndex',
    'SelectElementTest#shouldAllowOptionsToBeDeselectedByReturnedValue',
    'SelectElementTest#shouldAllowUserToDeselectAllWhenSelectSupportsMultipleSelections',
    'SelectElementTest#shouldAllowUserToDeselectOptionsByVisibleText',
    'TextHandlingTest#testShouldBeAbleToEnterDatesAfterFillingInOtherValuesFirst',
    'TouchFlickTest#testCanFlickHorizontally',
    'TouchFlickTest#testCanFlickHorizontallyFast',
    'TouchFlickTest#testCanFlickHorizontallyFastFromWebElement',
    'TouchFlickTest#testCanFlickHorizontallyFromWebElement',
    'TouchFlickTest#testCanFlickVertically',
    'TouchFlickTest#testCanFlickVerticallyFast',
    'TouchFlickTest#testCanFlickVerticallyFastFromWebElement',
    'TouchFlickTest#testCanFlickVerticallyFromWebElement',
    'TouchScrollTest#testCanScrollHorizontally',
    'TouchScrollTest#testCanScrollHorizontallyFromWebElement',
    'TouchScrollTest#testCanScrollVertically',
    'TouchScrollTest#testCanScrollVerticallyFromWebElement',
    'TouchSingleTapTest#testCanSingleTapOnALinkAndFollowIt',
    'TouchSingleTapTest#testCanSingleTapOnAnAnchorAndNotReloadThePage',
    'TypingTest#testAllPrintableKeys',
    'TypingTest#testArrowKeysAndPageUpAndDown',
    'TypingTest#testArrowKeysShouldNotBePrintable',
    'TypingTest#testFunctionKeys',
    'TypingTest#testHomeAndEndAndPageUpAndPageDownKeys',
    'TypingTest#testLowerCaseAlphaKeys',
    'TypingTest#testNumberpadKeys',
    'TypingTest#testNumericNonShiftKeys',
    'TypingTest#testNumericShiftKeys',
    'TypingTest#testShouldBeAbleToMixUpperAndLowerCaseLetters',
    'TypingTest#testShouldBeAbleToTypeCapitalLetters',
    'TypingTest#testShouldBeAbleToTypeQuoteMarks',
    'TypingTest#testShouldBeAbleToTypeTheAtCharacter',
    'TypingTest#testShouldBeAbleToUseArrowKeys',
    'TypingTest#testShouldFireFocusKeyEventsInTheRightOrder',
    'TypingTest#testShouldFireKeyDownEvents',
    'TypingTest#testShouldFireKeyPressEvents',
    'TypingTest#testShouldFireKeyUpEvents',
    'TypingTest#testShouldReportKeyCodeOfArrowKeys',
    'TypingTest#testShouldReportKeyCodeOfArrowKeysUpDownEvents',
    'TypingTest#testShouldTypeIntoInputElementsThatHaveNoTypeAttribute',
    'TypingTest#testShouldTypeLowerCaseLetters',
    'TypingTest#testSpecialSpaceKeys',
    'TypingTest#testUppercaseAlphaKeys',
    'TypingTest#testWillSimulateAKeyDownWhenEnteringTextIntoInputElements',
    'TypingTest#testWillSimulateAKeyDownWhenEnteringTextIntoTextAreas',
    'TypingTest#testWillSimulateAKeyPressWhenEnteringTextIntoInputElements',
    'TypingTest#testWillSimulateAKeyPressWhenEnteringTextIntoTextAreas',
    'TypingTest#testWillSimulateAKeyUpWhenEnteringTextIntoInputElements',
    'TypingTest#testWillSimulateAKeyUpWhenEnteringTextIntoTextAreas',
    'WindowSwitchingTest#testCanCloseWindowAndSwitchBackToMainWindow',
    'WindowSwitchingTest#testCanCloseWindowWhenMultipleWindowsAreOpen',
    'WindowSwitchingTest#testCanObtainAWindowHandle',
    'WindowSwitchingTest#testClosingOnlyWindowShouldNotCauseTheBrowserToHang',
    'WindowSwitchingTest#testShouldThrowNoSuchWindowException',
    'XPathElementFindingTest#testShouldBeAbleToFindManyElementsRepeatedlyByXPath',
    'XPathElementFindingTest#testShouldBeAbleToIdentifyElementsByClass',
    'XPathElementFindingTest#testShouldBeAbleToSearchForMultipleAttributes',
    'XPathElementFindingTest#testShouldFindElementsByXPath',
    'XPathElementFindingTest#testShouldFindSingleElementByXPath',
    'XPathElementFindingTest#testShouldLocateElementsWithGivenText',
    'XPathElementFindingTest#testShouldThrowAnExceptionWhenThereIsNoLinkToClick',
    'XPathElementFindingTest#testShouldThrowAnExceptionWhenThereIsNoLinkToClickAndItIsFoundWithXPath',
    'XPathElementFindingTest#testShouldThrowInvalidSelectorExceptionWhenXPathIsSyntacticallyInvalidInDriverFindElement',
    'XPathElementFindingTest#testShouldThrowInvalidSelectorExceptionWhenXPathIsSyntacticallyInvalidInDriverFindElements',
    'XPathElementFindingTest#testShouldThrowInvalidSelectorExceptionWhenXPathIsSyntacticallyInvalidInElementFindElement',
    'XPathElementFindingTest#testShouldThrowInvalidSelectorExceptionWhenXPathIsSyntacticallyInvalidInElementFindElements',
    'XPathElementFindingTest#testShouldThrowInvalidSelectorExceptionWhenXPathReturnsWrongTypeInDriverFindElement',
    'XPathElementFindingTest#testShouldThrowInvalidSelectorExceptionWhenXPathReturnsWrongTypeInDriverFindElements',
    'XPathElementFindingTest#testShouldThrowInvalidSelectorExceptionWhenXPathReturnsWrongTypeInElementFindElement',
    'XPathElementFindingTest#testShouldThrowInvalidSelectorExceptionWhenXPathReturnsWrongTypeInElementFindElements',
]


def _FilterPassedJavaTests(operating_system, chrome_revision):
  """Get the list of java tests that have passed in this environment.

  Args:
    operating_system: The operating system, one of linux, mac, win, android.
    chrome_revision: Chrome revision to test against.

  Returns:
    List of test names.
  """
  with open(os.path.join(_THIS_DIR, 'passed_java_tests.txt'), 'r') as f:
    globally_passed_tests = [line.strip('\n') for line in f]
  failed_tests = (
      _OS_NEGATIVE_FILTER[operating_system] +
      _REVISION_NEGATIVE_FILTER[chrome_revision]
  )
  passed_java_tests = []
  for java_test in globally_passed_tests:
    suite_name = java_test.split('#')[0]
    # Filter out failed tests by full name or suite name.
    if java_test not in failed_tests and suite_name not in failed_tests:
      passed_java_tests.append(java_test)
  return passed_java_tests


class BaseTestEnvironment(object):
  """Manages the environment java tests require to run."""

  def __init__(self, chrome_revision='HEAD'):
    """Initializes a desktop test environment.

    Args:
      chrome_revision: Optionally a chrome revision to run the tests against.
    """
    self._chrome_revision = chrome_revision

  def GlobalSetUp(self):
    """Sets up the global test environment state."""
    pass

  def GlobalTearDown(self):
    """Tears down the global test environment state."""
    pass

  def GetPassedJavaTests(self, os):
    """Get the list of java tests that have passed in this environment.

    Returns:
      List of test names.
    """
    raise NotImplementedError


class DesktopTestEnvironment(BaseTestEnvironment):
  """Manages the environment java tests require to run on Desktop."""

  #override
  def GetPassedJavaTests(self):
    return _FilterPassedJavaTests(util.GetPlatformName(), self._chrome_revision)


class AndroidTestEnvironment(DesktopTestEnvironment):
  """Manages the environment java tests require to run on Android."""

  def __init__(self, chrome_revision='HEAD'):
    super(AndroidTestEnvironment, self).__init__(chrome_revision)
    self._adb = None
    self._forwarder = None

  #override
  def GlobalSetUp(self):
    os.putenv('TEST_HTTP_PORT', str(ANDROID_TEST_HTTP_PORT))
    os.putenv('TEST_HTTPS_PORT', str(ANDROID_TEST_HTTPS_PORT))
    self._adb = android_commands.AndroidCommands()
    self._forwarder = forwarder.Forwarder(self._adb, 'Debug')
    self._forwarder.Run(
        [(ANDROID_TEST_HTTP_PORT, ANDROID_TEST_HTTP_PORT),
         (ANDROID_TEST_HTTPS_PORT, ANDROID_TEST_HTTPS_PORT)],
        valgrind_tools.BaseTool(), '127.0.0.1')

  #override
  def GlobalTearDown(self):
    if self._adb is not None:
      forwarder.Forwarder.KillDevice(self._adb, valgrind_tools.BaseTool())
    if self._forwarder is not None:
      self._forwarder.Close()

  #override
  def GetPassedJavaTests(self):
    return _FilterPassedJavaTests('android', self._chrome_revision)

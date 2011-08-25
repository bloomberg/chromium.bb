// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for print preview WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function PrintPreviewWebUITest() {}

PrintPreviewWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the sample page, cause print preview & call preLoad().
   * @type {string}
   * @override
   */
  browsePrintPreload: 'print_preview_hello_world_test.html',

  /**
   * Register a mock handler to ensure expectations are met and print preview
   * behaves correctly.
   * @type {Function}
   * @override
   */
  preLoad: function() {
    // TODO(scr) remove this after tests pass consistently.
    console.info('preLoad');

    /**
     * Create a handler class with empty methods to allow mocking to register
     * expectations and for registration of handlers with chrome.send.
     * @constructor
     */
    function MockPrintPreviewHandler() {}

    MockPrintPreviewHandler.prototype = {
      getDefaultPrinter: function() {},
      getPrinters: function() {},
      getPreview: function(settings) {},
      print: function(settings) {},
      getPrinterCapabilities: function(printerName) {},
      showSystemDialog: function() {},
      morePrinters: function() {},
      signIn: function() {},
      addCloudPrinter: function() {},
      manageCloudPrinters: function() {},
      manageLocalPrinters: function() {},
      closePrintPreviewTab: function() {},
      hidePreview: function() {},
      cancelPendingPrintRequest: function() {},
      saveLastPrinter: function(name, cloud_print_data) {},
    };

    // Create the actual mock and register stubs for methods expected to be
    // called before tests run.  Specific expectations can be made in the
    // tests themselves.
    var mockHandler = this.mockHandler = mock(MockPrintPreviewHandler);
    mockHandler.stubs().getDefaultPrinter().
        will(callFunction(function() {
          setDefaultPrinter('FooDevice');
        }));
    mockHandler.stubs().getPrinterCapabilities(NOT_NULL).
        will(callFunction(function() {
          updateWithPrinterCapabilities({
            disableColorOption: true,
            setColorAsDefault: true,
            disableCopiesOption: true,
          });
        }));
    var savedArgs = new SaveMockArguments();
    mockHandler.stubs().getPreview(savedArgs.match(NOT_NULL)).
        will(callFunctionWithSavedArgs(savedArgs, function(options) {
          updatePrintPreview('title', true, 1, JSON.parse(options).requestID);
        }));

    mockHandler.stubs().getPrinters().
        will(callFunction(function() {
          setUseCloudPrint(false, '');
          setPrinters([{
              printerName: 'FooName',
              deviceName: 'FooDevice',
            }, {
              printerName: 'BarName',
              deviceName: 'BarDevice',
            },
          ]);
        }));

    // Register mock as a handler of the chrome.send messages.
    registerMockMessageCallbacks(mockHandler, MockPrintPreviewHandler);

    /**
     * Create a class to hold global functions to watch for.
     * @constructor
     */
    function MockGlobals() {}

    MockGlobals.prototype = {
      updateWithPrinterCapabilities: function(settingInfo) {},
    };

    var mockGlobals = this.mockGlobals = mock(MockGlobals);
    mockGlobals.stubs().updateWithPrinterCapabilities(
        savedArgs.match(ANYTHING)).
            will(callGlobalWithSavedArgs(
                savedArgs, 'updateWithPrinterCapabilities'));

    // Register globals to mock out for us.
    registerMockGlobals(mockGlobals, MockGlobals);

    // Override checkCompatiblePluginExists to return a value consistent with
    // the state being tested and stub out the pdf viewer if it doesn't exist,
    // such as on non-official builds. When the plugin exists, use the real
    // thing.
    var self = this;
    window.addEventListener('DOMContentLoaded', function() {
      if (!this.checkCompatiblePluginExists()) {
        // TODO(scr) remove this after tests pass consistently.
        console.info('no PDF Plugin; providing fake methods.');
        this.createPDFPlugin = self.createPDFPlugin;
      }

      this.checkCompatiblePluginExists =
          self.checkCompatiblePluginExists;
    });
  },

  /**
   * Generate a real C++ class; don't typedef.
   * @type {?string}
   * @override
   */
  typedefCppFixture: null,

  /**
   * Create the PDF plugin or reload the existing one. This function replaces
   * createPDFPlugin defined in
   * chrome/browser/resources/print_preview/print_preview.js when there is no
   * official pdf plugin so that the WebUI logic can be tested. It creates and
   * attaches an HTMLDivElement to the |mainview| element with attributes and
   * empty methods, which are used by testing and that would be provided by the
   * HTMLEmbedElement when the PDF plugin exists.
   * @param {string} previewUid Preview unique identifier.
   */
  createPDFPlugin: function(previewUid) {
    var pdfViewer = $('pdf-viewer');
    if (pdfViewer)
      return;

    pdfViewer = document.createElement('div');
    pdfViewer.setAttribute('id', 'pdf-viewer');
    pdfViewer.setAttribute('type',
                           'application/x-google-chrome-print-preview-pdf');
    pdfViewer.setAttribute(
        'src', 'chrome://print/' + previewUid + '/print.pdf');
    pdfViewer.setAttribute('aria-live', 'polite');
    pdfViewer.setAttribute('aria-atomic', 'true');
    function fakeFunction() {}
    pdfViewer.onload = fakeFunction;
    pdfViewer.goToPage = fakeFunction;
    pdfViewer.removePrintButton = fakeFunction;
    pdfViewer.fitToHeight = fakeFunction;
    pdfViewer.grayscale = fakeFunction;
    $('mainview').appendChild(pdfViewer);
    onPDFLoad();
  },

  /**
   * Always return true so tests run on systems without plugin available.
   * @return {boolean} Always true.
   */
  checkCompatiblePluginExists: function() {
    return true;
  },
};

GEN('#include "base/command_line.h"');
GEN('#include "chrome/browser/ui/webui/web_ui_browsertest.h"');
GEN('#include "chrome/common/chrome_switches.h"');
GEN('');
GEN('class PrintPreviewWebUITest');
GEN('    : public WebUIBrowserTest {');
GEN(' protected:');
GEN('  // WebUIBrowserTest override.');
GEN('  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {');
GEN('    WebUIBrowserTest::SetUpCommandLine(command_line);');
GEN('    command_line->AppendSwitch(switches::kEnablePrintPreview);');
GEN('  }');
GEN('');
GEN('};');
GEN('');

/**
 * The expected length of the |printer-list| element.
 * @type {number}
 * @const
 */
var printerListMinLength = 2;

/**
 * The expected index of the "foo" printer returned by the stubbed handler.
 * @type {number}
 * @const
 */
var fooIndex = 0;

/**
 * The expected index of the "bar" printer returned by the stubbed handler.
 * @type {number}
 * @const
 */
var barIndex = 1;

// Test some basic assumptions about the print preview WebUI.
TEST_F('PrintPreviewWebUITest', 'TestPrinterList', function() {
  var printerList = $('printer-list');
  assertNotEquals(null, printerList);
  assertGE(printerList.options.length, printerListMinLength);
  expectEquals(fooIndex, printerList.selectedIndex);
  expectEquals('FooName', printerList.options[fooIndex].text,
               'fooIndex=' + fooIndex);
  expectEquals('FooDevice', printerList.options[fooIndex].value,
               'fooIndex=' + fooIndex);
  expectEquals('BarName', printerList.options[barIndex].text,
               'barIndex=' + barIndex);
  expectEquals('BarDevice', printerList.options[barIndex].value,
               'barIndex=' + barIndex);
});

// Test that the printer list is structured correctly after calling
// addCloudPrinters with an empty list.
TEST_F('PrintPreviewWebUITest', 'FLAKY_TestPrinterListCloudEmpty', function() {
  addCloudPrinters([]);
  var printerList = $('printer-list');
  assertNotEquals(null, printerList);
  expectEquals(localStrings.getString('cloudPrinters'),
               printerList.options[0].text);
  expectEquals(localStrings.getString('addCloudPrinter'),
               printerList.options[1].text);
});

// Test that the printer list is structured correctly after calling
// addCloudPrinters with a null list.
TEST_F('PrintPreviewWebUITest', 'FLAKY_TestPrinterListCloudNull', function() {
  addCloudPrinters(null);
  var printerList = $('printer-list');
  assertNotEquals(null, printerList);
  expectEquals(localStrings.getString('cloudPrinters'),
               printerList.options[0].text);
  expectEquals(localStrings.getString('signIn'),
               printerList.options[1].text);
});

// Test that the printer list is structured correctly after attempting to add
// individual cloud printers until no more can be added.
TEST_F('PrintPreviewWebUITest', 'FLAKY_TestPrinterListCloud', function() {
  var printerList = $('printer-list');
  assertNotEquals(null, printerList);
  var printer = new Object;
  printer['name'] = 'FooCloud';
  for (var i = 0; i < maxCloudPrinters; i++) {
    printer['id'] = String(i);
    addCloudPrinters([printer]);
    expectEquals(localStrings.getString('cloudPrinters'),
                 printerList.options[0].text);
    expectEquals('FooCloud', printerList.options[i + 1].text);
    expectEquals(String(i), printerList.options[i + 1].value);
  }
  printer['id'] = maxCloudPrinters + 1;
  addCloudPrinters([printer]);
  expectEquals('', printerList.options[maxCloudPrinters + 1].text);
  expectEquals(localStrings.getString('morePrinters'),
               printerList.options[maxCloudPrinters + 2].text);
});

/**
 * Verify that |section| visibility matches |visible|.
 * @param {HTMLDivElement} section The section to check.
 * @param {boolean} visible The expected state of visibility.
 */
function checkSectionVisible(section, visible) {
  assertNotEquals(null, section);
  expectEquals(section.classList.contains('visible'), visible,
               'section=' + section);
}

// Test that disabled settings hide the disabled sections.
TEST_F('PrintPreviewWebUITest', 'TestSectionsDisabled', function() {
  this.mockHandler.expects(once()).getPrinterCapabilities('FooDevice').
      will(callFunction(function() {
        updateWithPrinterCapabilities({
          disableColorOption: true,
          setColorAsDefault: true,
          disableCopiesOption: true,
          disableLandscapeOption: true,
        });
      }));

  updateControlsWithSelectedPrinterCapabilities();

  checkSectionVisible(layoutSettings.layoutOption_, false);
  checkSectionVisible(colorSettings.colorOption_, false);
  checkSectionVisible(copiesSettings.copiesOption_, false);
});

// Test that the color settings are set according to the printer capabilities.
TEST_F('PrintPreviewWebUITest', 'TestColorSettings', function() {
  this.mockHandler.expects(once()).getPrinterCapabilities('FooDevice').
      will(callFunction(function() {
        updateWithPrinterCapabilities({
          disableColorOption: false,
          setColorAsDefault: true,
          disableCopiesOption: false,
          disableLandscapeOption: false,
        });
      }));

  updateControlsWithSelectedPrinterCapabilities();
  expectTrue(colorSettings.colorRadioButton.checked);
  expectFalse(colorSettings.bwRadioButton.checked);

  this.mockHandler.expects(once()).getPrinterCapabilities('FooDevice').
      will(callFunction(function() {
        updateWithPrinterCapabilities({
          disableColorOption: false,
          setColorAsDefault: false,
          disableCopiesOption: false,
          disableLandscapeOption: false,
        });
      }));

  updateControlsWithSelectedPrinterCapabilities();
  expectFalse(colorSettings.colorRadioButton.checked);
  expectTrue(colorSettings.bwRadioButton.checked);
});

// Test that changing the selected printer updates the preview.
TEST_F('PrintPreviewWebUITest', 'TestPrinterChangeUpdatesPreview', function() {
  var savedArgs = new SaveMockArguments();
  this.mockHandler.expects(once()).getPreview(savedArgs.match(ANYTHING)).
      will(callFunctionWithSavedArgs(savedArgs, function(options) {
        updatePrintPreview('title', true, 2,
                           JSON.parse(options).requestID);
      }));

  this.mockGlobals.expects(once()).updateWithPrinterCapabilities(
      savedArgs.match(ANYTHING)).
          will(callGlobalWithSavedArgs(
              savedArgs, 'updateWithPrinterCapabilities'));

  var printerList = $('printer-list');
  assertNotEquals(null, printerList, 'printerList');
  assertGE(printerList.options.length, printerListMinLength);
  expectEquals(fooIndex, printerList.selectedIndex,
               'fooIndex=' + fooIndex);
  var oldLastPreviewRequestID = lastPreviewRequestID;
  ++printerList.selectedIndex;
  updateControlsWithSelectedPrinterCapabilities();
  expectNotEquals(oldLastPreviewRequestID, lastPreviewRequestID);
});

/**
 * Test fixture to test case when no PDF plugin exists.
 * @extends {PrintPreviewWebUITest}
 * @constructor
 */
function PrintPreviewNoPDFWebUITest() {}

PrintPreviewNoPDFWebUITest.prototype = {
  __proto__: PrintPreviewWebUITest.prototype,

  /**
   * Provide a typedef for C++ to correspond to JS subclass.
   * @type {?string}
   * @override
   */
  typedefCppFixture: 'PrintPreviewWebUITest',

  /**
   * Always return false to simulate failure and check expected error condition.
   * @return {boolean} Always false.
   * @override
   */
  checkCompatiblePluginExists: function() {
    return false;
  },
};

// Test that error message is displayed when plugin doesn't exist.
TEST_F('PrintPreviewNoPDFWebUITest', 'TestErrorMessage', function() {
  var errorButton = $('error-button');
  assertNotEquals(null, errorButton);
  expectFalse(errorButton.disabled);
  var errorText = $('error-text');
  assertNotEquals(null, errorText);
  expectFalse(errorText.classList.contains('hidden'));
});

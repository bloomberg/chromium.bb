// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
   function MockHandler() {
     this.__proto__ = MockHandler.prototype;
   };

   MockHandler.prototype = {
     'getDefaultPrinter': function() {
       console.log('getDefaultPrinter');
       setDefaultPrinter('FooDevice');
     },
     'getPrinters': function() {
       console.log('getPrinters');
       setPrinters([
                     {
                       'printerName': 'FooName',
                       'deviceName': 'FooDevice',
                     },
                     {
                       'printerName': 'BarName',
                       'deviceName': 'BarDevice',
                     },
                   ]);
     },
     'getPreview': function(settings) {
       console.log('getPreview(' + settings + ')');
       updatePrintPreview(1, 'title', true);
     },
     'print': function(settings) {
       console.log('print(' + settings + ')');
     },
     'getPrinterCapabilities': function(printer_name) {
       console.log('getPrinterCapabilities(' + printer_name + ')');
       updateWithPrinterCapabilities({
                                       'disableColorOption': true,
                                       'setColorAsDefault': true,
                                       'disableCopiesOption': true
                                     });
     },
     'showSystemDialog': function() {
       console.log('showSystemDialog');
     },
     'managePrinters': function() {
       console.log('managePrinters');
     },
     'closePrintPreviewTab': function() {
       console.log('closePrintPreviewTab');
     },
     'hidePreview': function() {
       console.log('hidePreview');
     },
   };

   function registerCallbacks() {
     console.log('registeringCallbacks');
     var mock_handler = new MockHandler();
     for (func in MockHandler.prototype) {
       if (typeof(mock_handler[func]) == 'function')
         registerMessageCallback(func,
                                 mock_handler,
                                 mock_handler[func]);
     }
   };

   function verifyBasicPrintPreviewButtons(printEnabled) {
     var printButton = $('print-button');
     assertTrue(printButton != null);
     var cancelButton = $('cancel-button');
     assertTrue(cancelButton != null);
   };

   registerCallbacks();

   internal = {
     'verifyBasicPrintPreviewButtons': verifyBasicPrintPreviewButtons,
   };
 })();

// Tests.
function testPrintPreview(printEnabled) {
  internal.verifyBasicPrintPreviewButtons(printEnabled);
  var printer_list = $('printer-list');
  assertTrue(printEnabled);
  assertTrue(printer_list.options.length >= 2);
  assertEquals('FooName', printer_list.options[0].text);
  assertEquals('FooDevice', printer_list.options[0].value);
  assertEquals('BarName', printer_list.options[1].text);
  assertEquals('BarDevice', printer_list.options[1].value);
}

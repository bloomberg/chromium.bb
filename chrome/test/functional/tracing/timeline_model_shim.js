// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function TimelineModelShim() {
  tracing.TimelineModel.apply(this, arguments);
}

TimelineModelShim.prototype = {
  __proto__: tracing.TimelineModel.prototype,

  invokeMethod: function(methodName, args) {
    var sendToPython = function(obj) {
      // We use sendJSON here because domAutomationController's send() chokes on
      // large amounts of data. Inside of send() it converts the arg to JSON and
      // invokes sendJSON. The JSON conversion is what fails. This way works
      // around the bad code, but note that the recieving python converts from
      // JSON before passing it back to the pyauto test.
      window.domAutomationController.sendJSON(
          JSON.stringify(obj)
      );
    };
    var result;
    try {
      result = this[methodName].apply(this, JSON.parse(args));
    } catch( e ) {
      var ret = {
        success: false,
        message: 'Unspecified error',
      };
      // We'll try sending the entire exception. If that doesn't work, it's ok.
      try {
        ret.exception = JSON.stringify(e);
      } catch(e2) {}
      if( typeof(e) == 'string' || e instanceof String ) {
        ret.message = e;
      } else {
        if( e.stack != undefined ) ret.stack = e.stack;
        if( e.message != undefined ) ret.message = e.message;
      }
      sendToPython(ret);
      throw e;
    }
    sendToPython({
      success: true,
      data: result
    });
  }
},

// This causes the PyAuto ExecuteJavascript call which executed this file to
// return.
window.domAutomationController.send('');

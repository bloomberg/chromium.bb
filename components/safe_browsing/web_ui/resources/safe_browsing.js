/* Copyright 2017 The Chromium Authors. All rights reserved.
* Use of this source code is governed by a BSD-style license that can be
* found in the LICENSE file. */

   cr.define('safe_browsing', function() {
     'use strict';

    function initialize() {
     chrome.send('expParamList',[]);
    }

   function addExperiment(result) {
   var resLength = result.length;
   var experimentsListFormatted="";

for (var i = 0; i < resLength; i+=2) {
    experimentsListFormatted += "<div>" + result[i+1] + " --- " + result [i] +
    "</div>";}
     $('experimentsList').innerHTML = experimentsListFormatted;
  }

    return {
     addExperiment: addExperiment,
      initialize: initialize,
    };

});

document.addEventListener('DOMContentLoaded', safe_browsing.initialize);

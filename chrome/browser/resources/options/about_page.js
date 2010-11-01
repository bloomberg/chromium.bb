// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;

  /**
   * Encapsulated handling of about page.
   */
  function AboutPage() {
    OptionsPage.call(this, 'about', templateData.aboutPage, 'aboutPage');
  }

  cr.addSingletonGetter(AboutPage);

  AboutPage.prototype = {
    // Inherit AboutPage from OptionsPage.
    __proto__: OptionsPage.prototype,

    // Initialize AboutPage.
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      $('checkNow').onclick = function(event) {
        chrome.send('CheckNow');
      };

      $('moreInfoButton').onclick = function(event) {
        $('aboutPageLessInfo').classList.add('hidden');
        $('aboutPageMoreInfo').classList.remove('hidden');
      };

      if (cr.commandLine.options['--bwsi']) {
        $('channelSelect').disabled = true;
      } else {
        $('channelSelect').onchange = function(event) {
          var channel = event.target.value;
          chrome.send('SetReleaseTrack', [channel]);
        };
      }

      // Notify the handler that the page is ready.
      chrome.send('PageReady');
    },

    // Update the Default Browsers section based on the current state.
    updateOSVersion_: function(versionString) {
      $('osVersion0').textContent = versionString;
      $('osVersion1').textContent = versionString;
    },

    updateStatus_: function(message) {
      $('updateStatus').textContent = message;
    },

    updateEnable_: function(enable) {
      $('checkNow').disabled = !enable;
    },

    // Updates the selected option in 'channelSelect' <select> element.
    updateSelectedOption_: function(value) {
      var options = $('channelSelect').querySelectorAll('option');
      for (var i = 0; i < options.length; i++) {
        var option = options[i];
        if (option.value == value) {
          option.selected = true;
        }
      }
    },

    // Changes the "check now" button to "restart now" button.
    changeToRestartButton_: function() {
      $('checkNow').textContent = localStrings.getString('restart_now');
      $('checkNow').disabled = false;
      $('checkNow').onclick = function(event) {
        chrome.send('RestartNow');
      };
    },
  };

  AboutPage.updateOSVersionCallback = function(versionString) {
    AboutPage.getInstance().updateOSVersion_(versionString);
  };

  AboutPage.updateStatusCallback = function(message) {
    AboutPage.getInstance().updateStatus_(message);
  };

  AboutPage.updateEnableCallback = function(enable) {
    AboutPage.getInstance().updateEnable_(enable);
  };

  AboutPage.updateSelectedOptionCallback = function(value) {
    AboutPage.getInstance().updateSelectedOption_(value);
  };

  AboutPage.setUpdateImage = function(state) {
    $('updateIcon').className= 'update-icon ' + state;
  };

  AboutPage.changeToRestartButton = function() {
    AboutPage.getInstance().changeToRestartButton_();
  };

  // Export
  return {
    AboutPage: AboutPage
  };

});

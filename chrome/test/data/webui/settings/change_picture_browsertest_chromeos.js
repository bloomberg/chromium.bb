// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for settings-change-picture. */

GEN_INCLUDE(['settings_page_browsertest.js']);

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
*/
function SettingsChangePictureBrowserTest() {
}

SettingsChangePictureBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/changePicture',

  /** @override */
  preLoad: function() {
    cr.define('settings_test', function() {
      var changePictureOptions = {
        /**
         * True if property changes should fire events for testing purposes.
         * @type {boolean}
         */
        notifyPropertyChangesForTest: true,
      };
      return {changePictureOptions: changePictureOptions};
    });
  }
};

// Times out on debug builders and may time out on memory bots because
// the Settings page can take several seconds to load in a Release build
// and several times that in a Debug build. See https://crbug.com/558434.
GEN('#if defined(MEMORY_SANITIZER) || !defined(NDEBUG)');
GEN('#define MAYBE_ChangePicture DISABLED_ChangePicture');
GEN('#else');
GEN('#define MAYBE_ChangePicture ChangePicture');
GEN('#endif');

// Runs change picture tests.
TEST_F('SettingsChangePictureBrowserTest', 'MAYBE_ChangePicture', function() {
  var basic = this.getPage('basic');
  assertTrue(!!basic);
  var peopleSection = this.getSection(basic, 'people');
  assertTrue(!!peopleSection);
  var peoplePage = peopleSection.querySelector('settings-people-page');
  assertTrue(!!peoplePage);
  var changePicture = peoplePage.$$('settings-change-picture');
  assertTrue(!!changePicture);

  /**
   * Registers a callback to be called once when the selected image URL changes.
   * @param {!function()} callback
   */
  function whenSelectedImageUrlChanged() {
    return new Promise(function(resolve, reject) {
      var handler = function() {
        changePicture.removeEventListener('selected-image-url_-changed',
                                          handler);
        resolve();
      };
      changePicture.addEventListener('selected-image-url_-changed', handler);
    });
  }

  suite('SettingsChangePicturePage', function() {
    setup(function() {
      // Reset the selected image to nothing.
      changePicture.selectedImageUrl_ = '';
    });

    test('select profile image', function() {
      var profileImage = changePicture.$$('#profile-image');
      assertTrue(!!profileImage);

      MockInteractions.tap(profileImage);

      return whenSelectedImageUrlChanged().then(function() {
        expectEquals(changePicture.selectedImageUrl_,
                     changePicture.profileImageUrl_);

        Polymer.dom.flush();
        expectTrue(profileImage.active);
      });
    });

    test('select old images', function() {
      // By default there is no old image.
      var oldImage = changePicture.$$('#old-image');
      assertFalse(!!oldImage);

      // The promise must start listening for the property change before
      // we make the fake API call.
      var promise = whenSelectedImageUrlChanged();

      settings.ChangePicturePage.receiveOldImage('fake-old-image.jpg');

      return promise.then(function() {
        // Expect the old image to be selected once an old image is sent via
        // the native interface.
        expectEquals(changePicture.selectedImageUrl_,
                     changePicture.oldImageUrl_);

        Polymer.dom.flush();
        var oldImage = changePicture.$$('#old-image');
        assertTrue(!!oldImage);
        expectTrue(oldImage.active);
      });
    });

    test('select first default image', function() {
      var firstDefaultImage = changePicture.$$('.default-image');
      console.log(firstDefaultImage);
      assertTrue(!!firstDefaultImage);

      MockInteractions.tap(firstDefaultImage);

      return whenSelectedImageUrlChanged().then(function() {
        // Expect the first default image to be selected.
        expectEquals(changePicture.selectedImageUrl_,
                     changePicture.defaultImages_[0].url);

        Polymer.dom.flush();
        expectTrue(firstDefaultImage.active);
      });
    });
  });

  // Run all registered tests.
  mocha.run();
});

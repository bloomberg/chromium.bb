// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_people_page_change_picture', function() {
  /**
   * @constructor
   * @implements {settings.ChangePictureBrowserProxy}
   * @extends {TestBrowserProxy}
   */
  var TestChangePictureBrowserProxy = function() {
    TestBrowserProxy.call(this, [
      'initialize',
      'selectDefaultImage',
      'selectOldImage',
      'selectProfileImage',
      'photoTaken',
      'chooseFile',
    ]);
  };

  TestChangePictureBrowserProxy.prototype = {
    __proto__: TestBrowserProxy.prototype,

    /** @override */
    initialize: function() {
      cr.webUIListenerCallback('profile-image-changed',
                               'fake-profile-image-url',
                               false /* selected */);

      var fakeDefaultImages = [
        {
          author: 'Author1',
          title: 'Title1',
          url: 'chrome://foo/1.png',
          website: 'http://foo1.com',
        },
        {
          author: 'Author2',
          title: 'Title2',
          url: 'chrome://foo/2.png',
          website: 'http://foo2.com',
        },
      ];
      cr.webUIListenerCallback('default-images-changed', fakeDefaultImages);

      this.methodCalled('initialize');
    },

    /** @override */
    selectDefaultImage: function(imageUrl) {
      cr.webUIListenerCallback('selected-image-changed', imageUrl);
      this.methodCalled('selectDefaultImage', [imageUrl]);
    },

    /** @override */
    selectOldImage: function() {
      cr.webUIListenerCallback('old-image-changed', 'fake-old-image.jpg');
      this.methodCalled('selectOldImage');
    },

    /** @override */
    selectProfileImage: function() {
      cr.webUIListenerCallback('profile-image-changed',
                               'fake-profile-image-url',
                               true /* selected */);
      this.methodCalled('selectProfileImage');
    },

    /** @override */
    photoTaken: function() {
      this.methodCalled('photoTaken');
    },

    /** @override */
    chooseFile: function() {
      this.methodCalled('chooseFile');
    },
  };

  function registerChangePictureTests() {
    suite('ChangePictureTests', function() {
      var changePicture = null;
      var browserProxy = null;
      var crPicturePane = null;
      var crPictureList = null;

      suiteSetup(function() {
        loadTimeData.overrideValues({
          profilePhoto: 'Fake Profile Photo description',
        });
      });

      setup(function() {
        browserProxy = new TestChangePictureBrowserProxy();
        settings.ChangePictureBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        changePicture = document.createElement('settings-change-picture');
        document.body.appendChild(changePicture);

        crPicturePane = changePicture.$$('cr-picture-pane');
        assertTrue(!!crPicturePane);

        crPictureList = changePicture.$$('cr-picture-list');
        assertTrue(!!crPictureList);

        changePicture.currentRouteChanged(settings.routes.CHANGE_PICTURE);

        return browserProxy.whenCalled('initialize').then(function() {
          Polymer.dom.flush();
        });
      });

      teardown(function() { changePicture.remove(); });

      test('ChangePictureSelectCamera', function() {
        // Force the camera to be absent, even if it's actually present.
        cr.webUIListenerCallback('camera-presence-changed', false);
        Polymer.dom.flush();

        return new Promise(function(resolve) {
          changePicture.async(resolve);
        }).then(function() {
          var camera = crPicturePane.$$('#camera');
          expectFalse(crPicturePane.cameraPresent);
          expectFalse(crPicturePane.cameraActive_);
          expectFalse(!!camera && camera.hidden);

          cr.webUIListenerCallback('camera-presence-changed', true);
          Polymer.dom.flush();
          return new Promise(function(resolve) {
            changePicture.async(resolve);
          });
        }).then(function() {
          var camera = crPicturePane.$$('#camera');
          expectTrue(crPicturePane.cameraPresent);
          expectFalse(crPicturePane.cameraActive_);
          expectFalse(!!camera && camera.hidden);

          var cameraImage = crPictureList.$.cameraImage;
          MockInteractions.tap(cameraImage);
          Polymer.dom.flush();
          return new Promise(function(resolve) {
            changePicture.async(resolve);
          });
        }).then(function() {
          var camera = crPicturePane.$$('#camera');
          expectTrue(crPicturePane.cameraActive_);
          assertTrue(!!camera && !camera.hidden);
          expectEquals(CrPicture.SelectionTypes.CAMERA,
                       changePicture.selectedItem_.dataset.type);
          var discard = crPicturePane.$$('#discard');
          expectTrue(!discard || discard.hidden);

          // Ensure that the camera is deactivated if user navigates away.
          changePicture.currentRouteChanged(settings.routes.BASIC);
          return new Promise(function(resolve) {
            changePicture.async(resolve);
          });
        }).then(function() {
          expectFalse(crPicturePane.cameraActive_);
        });
      });

      test('ChangePictureProfileImage', function() {
        var profileImage = crPictureList.$.profileImage;
        assertTrue(!!profileImage);

        expectEquals(null, changePicture.selectedItem_);
        MockInteractions.tap(profileImage);

        return browserProxy.whenCalled('selectProfileImage').then(function() {
          Polymer.dom.flush();

          expectEquals(CrPicture.SelectionTypes.PROFILE,
                       changePicture.selectedItem_.dataset.type);
          expectFalse(crPicturePane.cameraActive_);
          var discard = crPicturePane.$$('#discard');
          expectTrue(!discard || discard.hidden);

          // Ensure that the selection is restored after navigating away and
          // then back to the subpage.
          changePicture.currentRouteChanged(settings.routes.BASIC);
          changePicture.currentRouteChanged(settings.routes.CHANGE_PICTURE);
          expectEquals(null, changePicture.selectedItem_);
        });
      });

      test('ChangePictureOldImage', function() {
        // By default there is no old image and the element is hidden.
        var oldImage = crPictureList.$.oldImage;
        assertTrue(!!oldImage);
        assertTrue(oldImage.hidden);

        cr.webUIListenerCallback('old-image-changed', 'fake-old-image.jpg');
        Polymer.dom.flush();

        // Expect the old image to be selected once an old image is sent via
        // the native interface.
        expectEquals(CrPicture.SelectionTypes.OLD,
                     changePicture.selectedItem_.dataset.type);
        expectFalse(oldImage.hidden);
        expectFalse(crPicturePane.cameraActive_);
        var discard = crPicturePane.$$('#discard');
        assertTrue(!!discard);
        expectFalse(discard.hidden);
      });

      test('ChangePictureSelectFirstDefaultImage', function() {
        var firstDefaultImage = crPictureList.$$('img[data-type="default"]');
        assertTrue(!!firstDefaultImage);

        MockInteractions.tap(firstDefaultImage);

        return browserProxy.whenCalled('selectDefaultImage').then(
            function(args) {
              expectEquals('chrome://foo/1.png', args[0]);

              Polymer.dom.flush();
              expectEquals(CrPicture.SelectionTypes.DEFAULT,
                           changePicture.selectedItem_.dataset.type);
              expectEquals(firstDefaultImage, changePicture.selectedItem_);
              expectFalse(crPicturePane.cameraActive_);
              var discard = crPicturePane.$$('#discard');
              expectTrue(!discard || discard.hidden);

              // Now verify that arrow keys actually select the new image.
              browserProxy.resetResolver('selectDefaultImage');
              MockInteractions.pressAndReleaseKeyOn(
                  changePicture.selectedItem_, 39 /* right */);
              return browserProxy.whenCalled('selectDefaultImage');
            }).then(function(args) {
              expectEquals('chrome://foo/2.png', args[0]);
            });
      });

      test('ChangePictureRestoreImageAfterDiscard', function() {
        var firstDefaultImage = crPictureList.$$('img[data-type="default"]');
        assertTrue(!!firstDefaultImage);

        MockInteractions.tap(firstDefaultImage);

        return browserProxy.whenCalled('selectDefaultImage').then(function() {
          Polymer.dom.flush();
          expectEquals(firstDefaultImage, changePicture.selectedItem_);

          cr.webUIListenerCallback('old-image-changed', 'fake-old-image.jpg');

          Polymer.dom.flush();
          expectEquals(CrPicture.SelectionTypes.OLD,
                       changePicture.selectedItem_.dataset.type);

          var discardButton = crPicturePane.$$('#discard button');
          assertTrue(!!discardButton);
          MockInteractions.tap(discardButton);

          Polymer.dom.flush();
          expectEquals(firstDefaultImage, changePicture.selectedItem_);
        });
      });
    });
  }

  return {
    registerTests: function() {
      registerChangePictureTests();
    },
  };
});

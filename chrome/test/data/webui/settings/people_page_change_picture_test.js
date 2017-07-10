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
      var crPicturePreview = null;

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

        crPicturePreview = changePicture.$$('cr-picture-preview');
        assertTrue(!!crPicturePreview);

        changePicture.currentRouteChanged(settings.routes.CHANGE_PICTURE);

        return browserProxy.whenCalled('initialize').then(function() {
          Polymer.dom.flush();
        });
      });

      teardown(function() { changePicture.remove(); });

      test('ChangePictureSelectCamera', function() {
        var cameraIcon = changePicture.$.cameraImage;
        assertTrue(!!cameraIcon);

        // Force the camera to be absent, even if it's actually present.
        cr.webUIListenerCallback('camera-presence-changed', false);
        Polymer.dom.flush();

        return new Promise(function(resolve) {
          changePicture.async(resolve);
        }).then(function() {
          expectTrue(cameraIcon.hidden);
          expectFalse(crPicturePreview.cameraPresent);
          expectFalse(crPicturePreview.cameraActive_);

          cr.webUIListenerCallback('camera-presence-changed', true);
          Polymer.dom.flush();
          return new Promise(function(resolve) {
            changePicture.async(resolve);
          });
        }).then(function() {
          expectFalse(cameraIcon.hidden);
          expectTrue(crPicturePreview.cameraPresent);
          expectFalse(crPicturePreview.cameraActive_);

          MockInteractions.tap(cameraIcon);
          Polymer.dom.flush();
          return new Promise(function(resolve) {
            changePicture.async(resolve);
          });
        }).then(function() {
          expectFalse(cameraIcon.hidden);
          expectTrue(crPicturePreview.cameraActive_);
          expectEquals(CrPicture.SelectionTypes.CAMERA,
                       changePicture.selectedItem_.dataset.type);
          var discard = crPicturePreview.$$('#discard');
          expectTrue(!discard || discard.hidden);

          // Ensure that the camera is deactivated if user navigates away.
          changePicture.currentRouteChanged(settings.routes.BASIC);
          return new Promise(function(resolve) {
            changePicture.async(resolve);
          });
        }).then(function() {
          expectFalse(crPicturePreview.cameraActive_);
        });
      });

      test('ChangePictureProfileImage', function() {
        var profileImage = changePicture.$.profileImage;
        assertTrue(!!profileImage);

        expectEquals(undefined, changePicture.selectedItem_);
        MockInteractions.tap(profileImage);

        return browserProxy.whenCalled('selectProfileImage').then(function() {
          Polymer.dom.flush();

          expectEquals(CrPicture.SelectionTypes.PROFILE,
                       changePicture.selectedItem_.dataset.type);
          expectFalse(crPicturePreview.cameraActive_);
          var discard = crPicturePreview.$$('#discard');
          expectTrue(!discard || discard.hidden);

          // Ensure that the selection is restored after navigating away and
          // then back to the subpage.
          changePicture.currentRouteChanged(settings.routes.BASIC);
          changePicture.currentRouteChanged(settings.routes.CHANGE_PICTURE);
          expectEquals(CrPicture.SelectionTypes.PROFILE,
                       changePicture.selectedItem_.dataset.type);
        });
      });

      test('ChangePictureOldImage', function() {
        // By default there is no old image and the element is hidden.
        var oldImage = changePicture.$.oldImage;
        assertTrue(!!oldImage);
        assertTrue(oldImage.hidden);

        cr.webUIListenerCallback('old-image-changed', 'fake-old-image.jpg');
        Polymer.dom.flush();

        // Expect the old image to be selected once an old image is sent via
        // the native interface.
        expectEquals(CrPicture.SelectionTypes.OLD,
                     changePicture.selectedItem_.dataset.type);
        expectFalse(oldImage.hidden);
        expectFalse(crPicturePreview.cameraActive_);
        var discard = crPicturePreview.$$('#discard');
        assertTrue(!!discard);
        expectFalse(discard.hidden);
      });

      test('ChangePictureSelectFirstDefaultImage', function() {
        var firstDefaultImage = changePicture.$$('img[data-type="default"]');
        assertTrue(!!firstDefaultImage);

        MockInteractions.tap(firstDefaultImage);

        return browserProxy.whenCalled('selectDefaultImage').then(
            function(args) {
              expectEquals('chrome://foo/1.png', args[0]);

              Polymer.dom.flush();
              expectEquals(CrPicture.SelectionTypes.DEFAULT,
                           changePicture.selectedItem_.dataset.type);
              expectEquals(firstDefaultImage, changePicture.selectedItem_);
              expectFalse(crPicturePreview.cameraActive_);
              var discard = crPicturePreview.$$('#discard');
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
        var firstDefaultImage = changePicture.$$('img[data-type="default"]');
        assertTrue(!!firstDefaultImage);

        MockInteractions.tap(firstDefaultImage);

        return browserProxy.whenCalled('selectDefaultImage').then(function() {
          Polymer.dom.flush();
          expectEquals(firstDefaultImage, changePicture.selectedItem_);

          cr.webUIListenerCallback('old-image-changed', 'fake-old-image.jpg');

          Polymer.dom.flush();
          expectEquals(CrPicture.SelectionTypes.OLD,
                       changePicture.selectedItem_.dataset.type);

          var discardButton = crPicturePreview.$$('#discard button');
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

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('onboarding_ntp_background_test', function() {
  suite('NuxNtpBackgroundTest', function() {
    /** @type {!Array<!nux.NtpBackgroundData} */
    let backgrounds = [
      {
        id: 0,
        title: 'Art',
        /* Image URLs are set to actual static images to prevent requesting
         * an external image. */
        imageUrl: '../images/ntp_thumbnails/art.jpg',
        thumbnailClass: 'art',
      },
      {
        id: 1,
        title: 'Cityscape',
        imageUrl: '../images/ntp_thumbnails/cityscape.jpg',
        thumbnailClass: 'cityscape',
      },
    ];

    /** @type {NuxNtpBackgroundElement} */
    let testElement;

    /** @type {nux.NtpBackgroundProxy} */
    let testNtpBackgroundProxy;

    setup(function() {
      loadTimeData.overrideValues({
        ntpBackgroundDefault: 'Default',
      });

      testNtpBackgroundProxy = new TestNtpBackgroundProxy();
      nux.NtpBackgroundProxyImpl.instance_ = testNtpBackgroundProxy;
      testNtpBackgroundProxy.setBackgroundsList(backgrounds);

      PolymerTest.clearBody();
      testElement = document.createElement('nux-ntp-background');
      document.body.appendChild(testElement);

      testElement.onRouteEnter();
      return Promise.all([
        testNtpBackgroundProxy.whenCalled('getBackgrounds'),
      ]);
    });

    teardown(function() {
      testElement.remove();
    });

    test('test displaying default and custom background', function() {
      const options = testElement.shadowRoot.querySelectorAll(
          '.ntp-background-grid-button');
      assertEquals(3, options.length);

      // the first option should be the 'Default' option
      assertEquals(
          options[0].querySelector('.ntp-background-title').innerText,
          'Default');

      for (let i = 0; i < backgrounds.length; i++) {
        assertEquals(
            options[i + 1].querySelector('.ntp-background-title').innerText,
            backgrounds[i].title);
      }
    });

    test('test previewing a background and going back to default', function() {
      const options = testElement.shadowRoot.querySelectorAll(
          '.ntp-background-grid-button');

      options[1].click();
      return testNtpBackgroundProxy.whenCalled('preloadImage').then(() => {
        assertEquals(
            testElement.$.backgroundPreview.style.backgroundImage,
            `url("${backgrounds[0].imageUrl}")`);
        assertTrue(
            testElement.$.backgroundPreview.classList.contains('active'));

        // go back to the default option, and pretend all CSS transitions
        // have completed
        options[0].click();
        testElement.$.backgroundPreview.dispatchEvent(
            new Event('transitionend'));
        assertEquals(testElement.$.backgroundPreview.style.backgroundImage, '');
        assertFalse(
            testElement.$.backgroundPreview.classList.contains('active'));
      });
    });

    test('test disabling and enabling of the next button', function() {
      const nextButton = testElement.shadowRoot.querySelector('.action-button');
      assertTrue(nextButton.disabled);
      testElement.shadowRoot.querySelectorAll('.ntp-background-grid-button')[1]
          .click();
      assertFalse(nextButton.disabled);
    });

    test('test activating a background', function() {
      const options = testElement.shadowRoot.querySelectorAll(
          '.ntp-background-grid-button');

      options[1].click();
      assertFalse(options[0].hasAttribute('active'));
      assertEquals(options[0].getAttribute('aria-pressed'), 'false');
      assertTrue(options[1].hasAttribute('active'));
      assertEquals(options[1].getAttribute('aria-pressed'), 'true');
      assertFalse(options[2].hasAttribute('active'));
      assertEquals(options[2].getAttribute('aria-pressed'), 'false');
    });

    test('test setting the background when hitting next', function() {
      // select the first non-default option and hit 'Next'
      const options = testElement.shadowRoot.querySelectorAll(
          '.ntp-background-grid-button');
      options[1].click();
      testElement.$$('.action-button').click();
      return testNtpBackgroundProxy.whenCalled('setBackground').then((id) => {
        assertEquals(backgrounds[0].id, id);
      });
    });
  });
});

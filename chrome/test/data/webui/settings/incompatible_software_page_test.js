// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.IncompatibleSoftwareBrowserProxy} */
class TestIncompatibleSoftwareBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'requestIncompatibleSoftwareList',
      'startProgramUninstallation',
      'openURL',
    ]);

    /** @private {!Array<!settings.IncompatibleSoftware>} */
    this.incompatibleSoftware_ = [];
  }

  /** @override */
  requestIncompatibleSoftware() {
    this.methodCalled('requestIncompatibleSoftware');
    return Promise.resolve(this.incompatibleSoftware_);
  }

  /** @override */
  startProgramUninstallation(programName) {
    this.methodCalled('startProgramUninstallation', programName);
  }

  /** @override */
  openURL(url) {
    this.methodCalled('openURL', url);
  }

  /**
   * Sets the list of incompatible software returned by
   * requestIncompatibleSoftwareList().
   * @param {!Array<!settings.IncompatibleSoftware>} incompatibleSoftware
   */
  setIncompatibleSoftware(incompatibleSoftware) {
    this.incompatibleSoftware_ = incompatibleSoftware;
  }
}

suite('incompatibleSoftwareHandler', function() {
  let incompatibleSoftwarePage = null;

  /** @type {?TestIncompatibleSoftwareBrowserProxy} */
  let incompatibleSoftwareBrowserProxy = null;

  const incompatibleSoftware1 = {
    'name': 'Application 1',
    'type': 0,
    'url': '',
  };
  const incompatibleSoftware2 = {
    'name': 'Application 2',
    'type': 0,
    'url': '',
  };
  const incompatibleSoftware3 = {
    'name': 'Application 3',
    'type': 0,
    'url': '',
  };
  const learnMoreIncompatibleSoftware = {
    'name': 'Update Application',
    'type': 1,
    'url': 'chrome://update-url',
  };
  const updateIncompatibleSoftware = {
    'name': 'Update Application',
    'type': 1,
    'url': 'chrome://update-url',
  };

  /**
   * @param {!Array<settings.IncompatibleSoftware>}
   */
  function validateList(incompatibleSoftware) {
    const list = incompatibleSoftwarePage.shadowRoot.querySelectorAll(
        '.incompatible-software:not([hidden])');

    assertEquals(list.length, incompatibleSoftware.length);
  }

  setup(function() {
    incompatibleSoftwareBrowserProxy = new TestIncompatibleSoftwareBrowserProxy();
    settings.incompatibleSoftwareBrowserProxyImpl.instance_ =
        incompatibleSoftwareBrowserProxy;
  });

  /**
   * @param {boolean} hasAdminRights
   * @return {!Promise}
   */
  function initPage(hasAdminRights) {
    incompatibleSoftwareBrowserProxy.reset();
    PolymerTest.clearBody();

    loadTimeData.overrideValues({
      hasAdminRights: hasAdminRights,
    });

    incompatibleSoftwarePage =
        document.createElement('settings-incompatible-software-page');
    document.body.appendChild(incompatibleSoftwarePage);
    return incompatibleSoftwareBrowserProxy
        .whenCalled('requestIncompatibleSoftwareList')
        .then(function() {
          Polymer.dom.flush();
        });
  }

  test('openMultipleIncompatibleSoftware', function() {
    const multipleIncompatibleSoftwareTestList = [
      incompatibleSoftware1,
      incompatibleSoftware2,
      incompatibleSoftware3,
    ];

    incompatibleSoftwareBrowserProxy.setIncompatibleSoftware(
        multipleIncompatibleSoftwareTestList);

    return initPage(true).then(function() {
      validateList(multipleIncompatibleSoftwareTestList);
    });
  });

  test('startProgramUninstallation', function() {
    const singleIncompatibleSoftwareTestList = [
      incompatibleSoftware1,
    ];

    incompatibleSoftwareBrowserProxy.setIncompatibleSoftware(
        singleIncompatibleSoftwareTestList);

    return initPage(true /* hasAdminRights */)
        .then(function() {
          validateList(singleIncompatibleSoftwareTestList);

          // Retrieve the incompatible-software-item and tap it. It should be
          // visible.
          let item =
              incompatibleSoftwarePage.$$('.incompatible-software:not([hidden])');
          item.$$('.primary-button').click();

          return incompatibleSoftwareBrowserProxy.whenCalled(
              'startProgramUninstallation');
        })
        .then(function(programName) {
          assertEquals(incompatibleSoftware1.name, programName);
        });
  });

  test('learnMore', function() {
    const singleUpdateIncompatibleSoftwareTestList = [
      learnMoreIncompatibleSoftware,
    ];

    incompatibleSoftwareBrowserProxy.setIncompatibleSoftware(
        singleUpdateIncompatibleSoftwareTestList);

    return initPage(true /* hasAdminRights */)
        .then(function() {
          validateList(singleUpdateIncompatibleSoftwareTestList);

          // Retrieve the incompatible-software-item and tap it. It should be
          // visible.
          let item =
              incompatibleSoftwarePage.$$('.incompatible-software:not([hidden])');
          item.$$('.primary-button').click();

          return incompatibleSoftwareBrowserProxy.whenCalled('openURL');
        })
        .then(function(url) {
          assertEquals(updateIncompatibleSoftware.url, url);
        });
  });

  test('noAdminRights', function() {
    const eachTypeIncompatibleSoftwareTestList = [
      incompatibleSoftware1,
      learnMoreIncompatibleSoftware,
      updateIncompatibleSoftware,
    ];

    incompatibleSoftwareBrowserProxy.setIncompatibleSoftware(
        eachTypeIncompatibleSoftwareTestList);

    return initPage(false /* hasAdminRights */).then(function() {
      validateList(eachTypeIncompatibleSoftwareTestList);

      let items = incompatibleSoftwarePage.shadowRoot.querySelectorAll(
          '.incompatible-software:not([hidden])');

      assertEquals(items.length, 3);

      items.forEach(function(item, index) {
        // Just the name of the incompatible software is displayed inside a div
        // node. The <incompatible-software-item> component is not used.
        item.textContent.includes(
            eachTypeIncompatibleSoftwareTestList[index].name);
        assertNotEquals(item.nodeName, 'INCOMPATIBLE-SOFTWARE-ITEM');
      });
    });
  });
});

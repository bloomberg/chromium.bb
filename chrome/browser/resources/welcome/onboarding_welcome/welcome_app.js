// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The strings contained in the arrays should be valid DOM-element tag names.
 * @typedef {{
 *   'new-user': !Array<string>,
 *   'returning-user': !Array<string>
 * }}
 */
let NuxOnboardingModules;

Polymer({
  is: 'welcome-app',

  behaviors: [welcome.NavigationBehavior],

  /** @private {?welcome.Routes} */
  currentRoute_: null,

  // TODO(scottchen): instead of dummy, get data from finch/load time data.
  /** @private {NuxOnboardingModules} */
  modules_: {
    'new-user': ['h1', 'h1', 'h1'],
    'returning-user': ['h3', 'h3'],
  },

  /**
   * @param {welcome.Routes} route
   * @param {number} step
   * @private
   */
  onRouteChange: function(route, step) {
    // If the route changed, initialize the steps of modules for that route.
    if (this.currentRoute_ != route) {
      this.currentRoute_ = route;
      this.initializeModules(this.modules_[route]);
    }

    // If the specified step doesn't exist, that means there are no more steps.
    // In that case, go to NTP.
    if (!this.$$('#step-' + step))
      welcome.WelcomeBrowserProxyImpl.getInstance().goToNewTabPage();
    else  // Otherwise, go to the chosen step of that route.
      this.$.viewManager.switchView('step-' + step);
  },

  /** @param {!Array<string>} modules Array of valid DOM element names. */
  initializeModules: function(modules) {
    assert(this.currentRoute_);  // this.currentRoute_ should be set by now.
    if (this.currentRoute_ == welcome.Routes.LANDING)
      return;
    assert(modules);  // modules should be defined if on a valid route.

    // Remove all views except landing.
    this.$.viewManager
        .querySelectorAll('[slot="view"]:not([id="step-landing"])')
        .forEach(element => {
          element.remove();
        });

    modules.forEach((elementTagName, index) => {
      const element = document.createElement(elementTagName);
      element.id = 'step-' + (index + 1);
      element.setAttribute('slot', 'view');
      this.$.viewManager.appendChild(element);

      // TODO(scottchen): this is just to test routing works. Actual elements
      //     will have buttons that are responsible for navigation.
      element.textContent = index + 1;
      element.addEventListener('click', () => {
        this.navigateToNextStep();
      });
    });
  },
});

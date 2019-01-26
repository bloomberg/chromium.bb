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

/**
 * This list needs to be updated if new modules need to be supported in the
 * onboarding flow.
 * @const {!Set<string>}
 */
const MODULES_WHITELIST = new Set(
    ['nux-email', 'nux-google-apps', 'nux-set-as-default', 'signin-view']);

/**
 * This list needs to be updated if new modules that need step-indicators are
 * added.
 * @const {!Set<string>}
 */
const MODULES_NEEDING_INDICATOR =
    new Set(['nux-email', 'nux-google-apps', 'nux-set-as-default']);

Polymer({
  is: 'welcome-app',

  behaviors: [welcome.NavigationBehavior],

  /** @private {?welcome.Routes} */
  currentRoute_: null,

  /** @private {?PromiseResolver} */
  defaultCheckPromise_: null,

  /** @private {NuxOnboardingModules} */
  modules_: {
    'new-user': loadTimeData.getString('newUserModules').split(','),
    'returning-user': loadTimeData.getString('returningUserModules').split(','),
  },

  properties: {
    /** @private */
    modulesInitialized_: {
      type: Boolean,
      // Default to false so view-manager is hidden until views are initialized.
      value: false,
    },
  },

  /** @override */
  ready: function() {
    this.defaultCheckPromise_ = new PromiseResolver();

    /** @param {!nux.DefaultBrowserInfo} status */
    const defaultCheckCallback = status => {
      if (status.isDefault || !status.canBeDefault) {
        this.defaultCheckPromise_.resolve(false);
      } else if (!status.isDisabledByPolicy && !status.isUnknownError) {
        this.defaultCheckPromise_.resolve(true);
      } else {  // Unknown error.
        this.defaultCheckPromise_.resolve(false);
      }

      cr.removeWebUIListener(defaultCheckCallback);
    };

    cr.addWebUIListener('browser-default-state-changed', defaultCheckCallback);

    // TODO(scottchen): convert the request to cr.sendWithPromise
    // (see https://crbug.com/874520#c6).
    nux.NuxSetAsDefaultProxyImpl.getInstance().requestDefaultBrowserState();
  },

  /**
   * @param {welcome.Routes} route
   * @param {number} step
   * @private
   */
  onRouteChange: function(route, step) {
    const setStep = () => {
      // If the specified step doesn't exist, that means there are no more
      // steps. In that case, replace this page with NTP.
      if (!this.$$(`#step-${step}`)) {
        welcome.WelcomeBrowserProxyImpl.getInstance().goToNewTabPage(
            /* replace */ true);
      } else {  // Otherwise, go to the chosen step of that route.
        // At this point, views are ready to be shown.
        this.modulesInitialized_ = true;
        this.$.viewManager.switchView(
            `step-${step}`, 'fade-in', 'no-animation');
      }
    };

    // If the route changed, initialize the steps of modules for that route.
    if (this.currentRoute_ != route) {
      this.initializeModules(route).then(setStep);
    } else {
      setStep();
    }

    this.currentRoute_ = route;
  },

  /** @param {welcome.Routes} route */
  initializeModules: function(route) {
    // Remove all views except landing.
    this.$.viewManager
        .querySelectorAll('[slot="view"]:not([id="step-landing"])')
        .forEach(element => element.remove());

    // If it is on landing route, end here.
    if (route == welcome.Routes.LANDING) {
      return Promise.resolve();
    }

    let modules = this.modules_[route];
    assert(modules);  // Modules should be defined if on a valid route.

    // Wait until the default-browser state and bookmark visibility are known
    // before anything initializes.
    return Promise
        .all([
          this.defaultCheckPromise_.promise,
          nux.BookmarkBarManager.getInstance().initialized,
        ])
        .then(args => {
          const canSetDefault = args[0];

          modules = modules.filter(module => {
            if (module == 'nux-set-as-default') {
              return canSetDefault;
            }

            if (module == 'nux-email') {
              // Show email module in en-US only until email recommendations
              // for other locales is figured out.
              return navigator.language == 'en-US';
            }

            return true;
          });

          const indicatorElementCount = modules.reduce((count, module) => {
            return count += MODULES_NEEDING_INDICATOR.has(module) ? 1 : 0;
          }, 0);

          let indicatorActiveCount = 0;
          modules.forEach((elementTagName, index) => {
            // Makes sure the module specified by the feature configuration is
            // whitelisted.
            assert(MODULES_WHITELIST.has(elementTagName));

            const element = document.createElement(elementTagName);
            element.id = 'step-' + (index + 1);
            element.setAttribute('slot', 'view');
            this.$.viewManager.appendChild(element);

            if (MODULES_NEEDING_INDICATOR.has(elementTagName)) {
              element.indicatorModel = {
                total: indicatorElementCount,
                active: indicatorActiveCount++,
              };
            }
          });
        });
  },
});

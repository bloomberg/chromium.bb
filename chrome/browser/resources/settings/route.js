// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /**
   * Class for navigable routes. May only be instantiated within this file.
   * @constructor
   * @param {string} url
   * @private
   */
  var Route = function(url) {
    this.url = url;

    /** @private {?settings.Route} */
    this.parent_ = null;

    // Below are all legacy properties to provide compatibility with the old
    // routing system. TODO(tommycli): Remove once routing refactor complete.
    this.page = '';
    this.section = '';
    /** @type {!Array<string>} */ this.subpage = [];
  };

  Route.prototype = {
    /**
     * Returns a new Route instance that's a child of this route.
     * @param {string} path Extends this route's path if it doesn't contain a
     *     leading slash.
     * @param {string=} opt_subpageName
     * @return {!settings.Route}
     * @private
     */
    createChild: function(path, opt_subpageName) {
      assert(path);

      // |path| extends this route's path if it doesn't have a leading slash.
      // If it does have a leading slash, it's just set as the new route's URL.
      var newUrl = path[0] == '/' ? path : this.url + '/' + path;

      var route = new Route(newUrl);
      route.parent_ = this;
      route.page = this.page;
      route.section = this.section;
      route.subpage = this.subpage.slice();  // Shallow copy.

      if (opt_subpageName)
        route.subpage.push(opt_subpageName);

      return route;
    },

    /**
     * Returns a new Route instance that's a child dialog of this route.
     * @param {string} url
     * @param {string} dialogName
     * @return {!settings.Route}
     * @private
     */
    createDialog: function(url, dialogName) {
      var route = this.createChild(url);
      route.dialog = dialogName;
      return route;
    },

    /**
     * Returns a new Route instance that's a child section of this route.
     * TODO(tommycli): Remove once we've obsoleted the concept of sections.
     * @param {string} url
     * @param {string} section
     * @return {!settings.Route}
     * @private
     */
    createSection: function(url, section) {
      var route = this.createChild(url);
      route.section = section;
      return route;
    },

    /**
     * Returns true if this route is a descendant of the parameter.
     * @param {!settings.Route} route
     * @return {boolean}
     */
    isDescendantOf: function(route) {
      for (var parent = this.parent_; parent != null; parent = parent.parent_) {
        if (route == parent)
          return true;
      }

      return false;
    },
  };

  // Abbreviated variable for easier definitions.
  var r = Route;

  // Root pages.
  r.BASIC = new Route('/');
  r.BASIC.page = 'basic';
  r.ADVANCED = new Route('/advanced');
  r.ADVANCED.page = 'advanced';
  r.ABOUT = new Route('/help');
  r.ABOUT.page = 'about';

<if expr="chromeos">
  r.INTERNET = r.BASIC.createSection('/internet', 'internet');
  r.NETWORK_DETAIL = r.INTERNET.createChild('/networkDetail', 'network-detail');
  r.KNOWN_NETWORKS = r.INTERNET.createChild('/knownNetworks', 'known-networks');
</if>

  r.APPEARANCE = r.BASIC.createSection('/appearance', 'appearance');
  r.FONTS = r.APPEARANCE.createChild('/fonts', 'appearance-fonts');

  r.DEFAULT_BROWSER =
      r.BASIC.createSection('/defaultBrowser', 'defaultBrowser');

  r.SEARCH = r.BASIC.createSection('/search', 'search');
  r.SEARCH_ENGINES = r.SEARCH.createChild('/searchEngines', 'search-engines');

  r.ON_STARTUP = r.BASIC.createSection('/onStartup', 'onStartup');

  r.PEOPLE = r.BASIC.createSection('/people', 'people');
  r.SYNC = r.PEOPLE.createChild('/syncSetup', 'sync');
<if expr="not chromeos">
  r.MANAGE_PROFILE = r.PEOPLE.createChild('/manageProfile', 'manageProfile');
</if>
<if expr="chromeos">
  r.CHANGE_PICTURE = r.PEOPLE.createChild('/changePicture', 'changePicture');
  r.QUICK_UNLOCK_AUTHENTICATE =
      r.PEOPLE.createChild('/quickUnlock/authenticate',
                           'quick-unlock-authenticate');
  r.QUICK_UNLOCK_CHOOSE_METHOD =
      r.PEOPLE.createChild('/quickUnlock/chooseMethod',
                           'quick-unlock-choose-method');
  r.QUICK_UNLOCK_SETUP_PIN =
      r.QUICK_UNLOCK_CHOOSE_METHOD.createChild('/quickUnlock/setupPin',
                                               'quick-unlock-setup-pin');
  r.ACCOUNTS = r.PEOPLE.createChild('/accounts', 'users');

  r.DEVICE = r.BASIC.createSection('/device', 'device');
  r.TOUCHPAD = r.DEVICE.createChild('/pointer-overlay', 'touchpad');
  r.KEYBARD = r.DEVICE.createChild('/keyboard-overlay', 'keyboard');
  r.DISPLAY = r.DEVICE.createChild('/display', 'display');
</if>

  r.PRIVACY = r.ADVANCED.createSection('/privacy', 'privacy');
  r.CERTIFICATES =
      r.PRIVACY.createChild('/certificates', 'manage-certificates');
  r.CLEAR_BROWSER_DATA =
      r.PRIVACY.createDialog('/clearBrowserData', 'clear-browsing-data');
  r.SITE_SETTINGS = r.PRIVACY.createChild('/siteSettings', 'site-settings');
  r.SITE_SETTINGS_ALL = r.SITE_SETTINGS.createChild('all', 'all-sites');
  r.SITE_SETTINGS_ALL_DETAILS =
      r.SITE_SETTINGS_ALL.createChild('details', 'site-details');

  r.SITE_SETTINGS_HANDLERS = r.SITE_SETTINGS.createChild(
      'handlers', 'protocol-handlers');

  // TODO(tommicli): Find a way to refactor these repetitive category routes.
  r.SITE_SETTINGS_AUTOMATIC_DOWNLOADS = r.SITE_SETTINGS.createChild(
      'automaticDownloads', 'site-settings-category-automatic-downloads');
  r.SITE_SETTINGS_BACKGROUND_SYNC = r.SITE_SETTINGS.createChild(
      'backgroundSync', 'site-settings-category-background-sync');
  r.SITE_SETTINGS_CAMERA = r.SITE_SETTINGS.createChild(
      'camera', 'site-settings-category-camera');
  r.SITE_SETTINGS_COOKIES = r.SITE_SETTINGS.createChild(
      'cookies', 'site-settings-category-cookies');
  r.SITE_SETTINGS_FULLSCREEN = r.SITE_SETTINGS.createChild(
      'fullscreen', 'site-settings-category-fullscreen');
  r.SITE_SETTINGS_IMAGES = r.SITE_SETTINGS.createChild(
      'images', 'site-settings-category-images');
  r.SITE_SETTINGS_JAVASCRIPT = r.SITE_SETTINGS.createChild(
      'javascript', 'site-settings-category-javascript');
  r.SITE_SETTINGS_KEYGEN = r.SITE_SETTINGS.createChild(
      'keygen', 'site-settings-category-keygen');
  r.SITE_SETTINGS_LOCATION = r.SITE_SETTINGS.createChild(
      'location', 'site-settings-category-location');
  r.SITE_SETTINGS_MICROPHONE = r.SITE_SETTINGS.createChild(
      'microphone', 'site-settings-category-microphone');
  r.SITE_SETTINGS_NOTIFICATIONS = r.SITE_SETTINGS.createChild(
      'notifications', 'site-settings-category-notifications');
  r.SITE_SETTINGS_PLUGINS = r.SITE_SETTINGS.createChild(
      'plugins', 'site-settings-category-plugins');
  r.SITE_SETTINGS_POPUPS = r.SITE_SETTINGS.createChild(
      'popups', 'site-settings-category-popups');
  r.SITE_SETTINGS_UNSANDBOXED_PLUGINS = r.SITE_SETTINGS.createChild(
      'unsandboxedPlugins', 'site-settings-category-unsandboxed-plugins');

  r.SITE_SETTINGS_AUTOMATIC_DOWNLOADS_DETAILS =
      r.SITE_SETTINGS_AUTOMATIC_DOWNLOADS.createChild('details',
                                                      'site-details');
  r.SITE_SETTINGS_BACKGROUND_SYNC_DETAILS =
      r.SITE_SETTINGS_BACKGROUND_SYNC.createChild('details', 'site-details');
  r.SITE_SETTINGS_CAMERA_DETAILS =
      r.SITE_SETTINGS_CAMERA.createChild('details', 'site-details');
  r.SITE_SETTINGS_COOKIES_DETAILS =
      r.SITE_SETTINGS_COOKIES.createChild('details', 'site-details');
  r.SITE_SETTINGS_FULLSCREEN_DETAILS =
      r.SITE_SETTINGS_FULLSCREEN.createChild('details', 'site-details');
  r.SITE_SETTINGS_IMAGES_DETAILS =
      r.SITE_SETTINGS_IMAGES.createChild('details', 'site-details');
  r.SITE_SETTINGS_JAVASCRIPT_DETAILS =
      r.SITE_SETTINGS_JAVASCRIPT.createChild('details', 'site-details');
  r.SITE_SETTINGS_KEYGEN_DETAILS =
      r.SITE_SETTINGS_KEYGEN.createChild('details', 'site-details');
  r.SITE_SETTINGS_LOCATION_DETAILS =
      r.SITE_SETTINGS_LOCATION.createChild('details', 'site-details');
  r.SITE_SETTINGS_MICROPHONE_DETAILS =
      r.SITE_SETTINGS_MICROPHONE.createChild('details', 'site-details');
  r.SITE_SETTINGS_NOTIFICATIONS_DETAILS =
      r.SITE_SETTINGS_NOTIFICATIONS.createChild('details', 'site-details');
  r.SITE_SETTINGS_PLUGINS_DETAILS =
      r.SITE_SETTINGS_PLUGINS.createChild('details', 'site-details');
  r.SITE_SETTINGS_POPUPS_DETAILS =
      r.SITE_SETTINGS_POPUPS.createChild('details', 'site-details');
  r.SITE_SETTINGS_UNSANDBOXED_PLUGINS_DETAILS =
      r.SITE_SETTINGS_UNSANDBOXED_PLUGINS.createChild('details',
                                                      'site-details');

<if expr="chromeos">
  r.DATETIME = r.ADVANCED.createSection('/dateTime', 'dateTime');

  r.BLUETOOTH = r.ADVANCED.createSection('/bluetooth', 'bluetooth');
  r.BLUETOOTH_ADD_DEVICE =
      r.BLUETOOTH.createChild('/bluetoothAddDevice', 'bluetooth-add-device');
  r.BLUETOOTH_PAIR_DEVICE = r.BLUETOOTH_ADD_DEVICE.createChild(
      'bluetoothPairDevice', 'bluetooth-pair-device');
</if>

  r.PASSWORDS = r.ADVANCED.createSection('/passwords', 'passwordsAndForms');
  r.AUTOFILL = r.PASSWORDS.createChild('/autofill', 'manage-autofill');
  r.MANAGE_PASSWORDS =
      r.PASSWORDS.createChild('/managePasswords', 'manage-passwords');

  r.LANGUAGES = r.ADVANCED.createSection('/languages', 'languages');
  r.LANGUAGES_DETAIL = r.LANGUAGES.createChild('edit', 'language-detail');
  r.MANAGE_LANGUAGES =
      r.LANGUAGES.createChild('/manageLanguages', 'manage-languages');
<if expr="chromeos">
  r.INPUT_METHODS =
      r.LANGUAGES.createChild('/inputMethods', 'manage-input-methods');
</if>
<if expr="not is_macosx">
  r.EDIT_DICTIONARY =
      r.LANGUAGES.createChild('/editDictionary', 'edit-dictionary');
</if>

  r.DOWNLOADS = r.ADVANCED.createSection('/downloadsDirectory', 'downloads');

  r.PRINTING = r.ADVANCED.createSection('/printing', 'printing');
  r.CLOUD_PRINTERS = r.PRINTING.createChild('/cloudPrinters', 'cloud-printers');
<if expr="chromeos">
  r.CUPS_PRINTERS = r.PRINTING.createChild('/cupsPrinters', 'cups-printers');
</if>

  r.ACCESSIBILITY = r.ADVANCED.createSection('/accessibility', 'a11y');
  r.SYSTEM = r.ADVANCED.createSection('/system', 'system');
  r.RESET = r.ADVANCED.createSection('/reset', 'reset');

<if expr="chromeos">
  r.INPUT_METHODS =
      r.LANGUAGES.createChild('/inputMethods', 'manage-input-methods');
  r.DETAILED_BUILD_INFO =
      r.ABOUT.createChild('/help/details', 'detailed-build-info');
  r.DETAILED_BUILD_INFO.section = 'about';
</if>

  return {
    Route: Route,
  };
});

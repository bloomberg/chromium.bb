// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file contains the implementation of the Chrome extension
 * infobars API for the CEEE Firefox add-on.  This file is loaded by the
 * overlay.xul file, and requires that overlay.js has already been loaded.
 *
 * @supported Firefox 3.x
 */

/**
 * Place-holder namespace-object for helper functions used in this file.
 * @private
 */
var CEEE_infobars_internal_ = CEEE_infobars_internal_ || {
  /** Reference to the instance object for the ceee. @private */
  ceeeInstance_: null
};

/**
 * @const
 */
CEEE_infobars_internal_.NOTIFICATION_ID = 'ceeeNotification';

/**
 * @const
 */
CEEE_infobars_internal_.ANONID = 'anonid';

/**
 * @const
 */
CEEE_infobars_internal_.PARENT_ANONID = 'details';

/**
 * @const
 */
CEEE_infobars_internal_.ID_PREFIX = 'ceee-infobar-';

/**
 * Get a File object for the given user script.  Its assumed that the user
 * script file name is relative to the Chrome Extension root.
 *
 * @param {!string} path Relative file name of the user script.
 * @return {string} Absolute URL for the user script.
 */
function CEEE_infobars_getUserScriptUrl(path) {
  var ceee = CEEE_infobars_internal_.ceeeInstance_;
  var id = ceee.getToolstripExtensionId();
  return 'chrome-extension://' + id + '/' + path;
}

/**
 * Implementation of experimental.infobars.show method.
 * @param {string} cmd Command name, 'experimenta.infobars.show' expected
 * @param {Object} data Additional call info, includes args field with
 *     json-encoded arguments.
 */
CEEE_infobars_internal_.show = function(cmd, data){
  var ceee = CEEE_infobars_internal_.ceeeInstance_;
  var args = CEEE_json.decode(data.args)[0];
  var tabId = args.tabId;
  var path = args.path;
  var url = CEEE_infobars_getUserScriptUrl(path);
  var tabbrowser = null;
  var browser = null;
  if ('number' != typeof tabId || tabId < 0) {
    tabbrowser = document.getElementById(CEEE_globals.MAIN_BROWSER_ID);
    browser = tabbrowser.selectedBrowser;
  } else {
    var tabInfo = CEEE_mozilla_tabs.findTab(tabId);
    tabbrowser = tabInfo.tabbrowser;
    browser = tabbrowser.getBrowserAtIndex(tabInfo.index);
  }
  var impl = CEEE_infobars_internal_;
  var notificationBox = tabbrowser.getNotificationBox(browser);
  var notification = notificationBox.appendNotification(
    '',  // message
    impl.NOTIFICATION_ID, // identifier
    null,  // icon
    notificationBox.PRIORITY_INFO_HIGH,
    null  // buttons
  );
  var parent = document.getAnonymousElementByAttribute(notification,
      impl.ANONID, impl.PARENT_ANONID);
  // We don't need the default content there. Removing icon, message text etc.
  while (parent.firstChild) {
    parent.removeChild(parent.lastChild);
  }

  // Let's assign window id first and use it as base for chrome frame id.
  var result = ceee.getWindowModule().buildWindowValue(
      new NotificationAsWindow(notification, parent));
  var id = impl.ID_PREFIX + result.id;
  var cf = CEEE_infobars_createChromeFrame_(parent, id, url);
  var infobars =
      notification.ownerDocument.defaultView.CEEE_infobars_internal_;
  infobars.lookupMap_ = infobars.lookupMap_ || {};
  infobars.lookupMap_[id] = notificationBox.id;
  return result;
};
CEEE_infobars_internal_.show.CMD = 'experimental.infobars.show';

CEEE_infobars_internal_.lookup = function(id) {
  var ceee = CEEE_infobars_internal_.ceeeInstance_;
  var impl = CEEE_infobars_internal_;
  var e = CEEE_mozilla_windows.service.getEnumerator(
      CEEE_mozilla_windows.WINDOW_TYPE);
  while (e.hasMoreElements()) {
    var win = e.getNext();
    var infobars = win.CEEE_infobars_internal_;
    var notificationBoxId = infobars.lookupMap_ &&
        infobars.lookupMap_[impl.ID_PREFIX + id];
    if (!notificationBoxId) {
      continue;
    }
    var doc = win.document;
    var notificationBox = doc.getElementById(notificationBoxId);
    var notifications = notificationBox.allNotifications;
    for (var i = 0, curr; curr = notifications[i]; ++i) {
      var parent = document.getAnonymousElementByAttribute(curr,
          impl.ANONID, impl.PARENT_ANONID);
      if (parent.firstChild.id == impl.ID_PREFIX + id) {
        return new NotificationAsWindow(curr, parent);
      }
    }
    // Notification not found. Remove from lookup cache
    delete infobars.lookupMap_[impl.ID_PREFIX + id];
    // Once we had it in lookup map, no point to continue search
    break;
  }
  return null;
};

/**
 * Create the ChromeFrame element for infobar content.
 * @private
 */
function CEEE_infobars_createChromeFrame_(parent, id, url) {
  var ceee = CEEE_infobars_internal_.ceeeInstance_;
  var cf = parent.ownerDocument.createElementNS('http://www.w3.org/1999/xhtml',
      'embed');
  cf.id = id;
  // TODO(ibazarny@google.com): the height here is arbitrary.  We will
  // need to size appropriately when we have more info.
  cf.setAttribute('height', '35');
  cf.setAttribute('flex', '1');
  cf.setAttribute('type', 'application/chromeframe');
  cf.setAttribute('privileged_mode', '1');
  cf.setAttribute('chrome_extra_arguments',
                  '--enable-experimental-extension-apis');
  cf.setAttribute('src', url);
  parent.appendChild(cf);
  return cf;
}

/**
 * Initialization routine for the CEEE infobars API module.
 * @param {!Object} ceeeInstance Reference to the global ceee instance.
 * @param {!Object} windowModule Window management module, used to register
 *     fake window lookup so we can turn notifications into windows.
 * @return {Object} Reference to the infobars module.
 * @public
 */
function CEEE_initialize_infobars(ceeeInstance, windowModule) {
  CEEE_infobars_internal_.ceeeInstance_ = ceeeInstance;
  var infobars = CEEE_infobars_internal_;
  ceeeInstance.registerExtensionHandler(infobars.show.CMD,
                                        infobars,
                                        infobars.show);
  windowModule.registerLookup(infobars.lookup);
  return infobars;
}

/**
 * Make notification look like Window, so that window api module can work with
 * it.
 * @constructor
 */
function NotificationAsWindow(notification, cfContainer){
  this.isAdapter = true;
  this.notification_ = notification;
  this.cfContainer_ = cfContainer;
}

NotificationAsWindow.prototype.__defineGetter__('screenX', function(){
  return this.cfContainer_.boxObject.screenX;
});

NotificationAsWindow.prototype.__defineGetter__('screenY', function(){
  return this.cfContainer_.boxObject.screenY;
});

NotificationAsWindow.prototype.__defineGetter__('outerWidth', function(){
  return this.cfContainer_.boxObject.width;
});

NotificationAsWindow.prototype.__defineGetter__('outerHeight', function(){
  return this.cfContainer_.boxObject.height;
});

NotificationAsWindow.prototype.__defineGetter__(
    CEEE_mozilla_windows.WINDOW_ID,
    function(){
      return this.notification_[CEEE_mozilla_windows.WINDOW_ID];
    });

NotificationAsWindow.prototype.__defineSetter__(
    CEEE_mozilla_windows.WINDOW_ID,
    function(value){
      this.notification_[CEEE_mozilla_windows.WINDOW_ID] = value;
    });

NotificationAsWindow.prototype.close = function() {
  this.notification_.close();
};

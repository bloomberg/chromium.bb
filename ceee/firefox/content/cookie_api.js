// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file contains the implementation of the Chrome extension
 * cookies APIs for the CEEE Firefox add-on.  This file is loaded by the
 * overlay.xul file, and requires that overlay.js has already been loaded.
 *
 * @supported Firefox 3.x
 */


/**
 * "cookie-changed" Mozilla event observer.
 * @param {function()} handler Handle event function.
 * @constructor
 */
function CEEE_CookieChangeObserver(handler) {
  this.observe = handler;
}

/**
 * XPCOM interface function.
 * @param {nsIIDRef} iid Interface ID.
 * @return this object when the queried interface is nsIObserver.
 */
CEEE_CookieChangeObserver.prototype.QueryInterface = function(iid) {
  if (iid.equals(Components.interfaces.nsIObserver) ||
      iid.equals(Components.interfaces.nsISupports)) {
    return this;
  }
  throw Components.results.NS_NOINTERFACE;
};

/**
 * Registers the observer in the Mozilla Observer service.
 * @private
 */
CEEE_CookieChangeObserver.prototype.register_ = function() {
  var observerService = Components.classes['@mozilla.org/observer-service;1']
                        .getService(Components.interfaces.nsIObserverService);
  observerService.addObserver(this, 'cookie-changed', false);
};

/**
 * Unregisters the observer in the Mozilla Observer service.
 * @private
 */
CEEE_CookieChangeObserver.prototype.unregister_ = function() {
  var observerService = Components.classes['@mozilla.org/observer-service;1']
                        .getService(Components.interfaces.nsIObserverService);
  observerService.removeObserver(this, 'cookie-changed');
};


/**
 * chrome.cookies interface implementation.
 * @type Object
 */
var CEEE_cookie_internal_ = CEEE_cookie_internal_ || {
  /**
    * Reference to the instance object for the ceee.
    * @private
    */
  ceeeInstance_: null,

  // Cookie store IDs.
  /** @const */ STORE_PRIVATE: 'private',
  /** @const */ STORE_NORMAL: 'normal'
};

/**
 * Initializes the cookies module during onLoad message from the browser.
 * @public
 */
CEEE_cookie_internal_.onLoad = function() {
  var cookies = this;
  this.cookieChangeObserver_ = new CEEE_CookieChangeObserver(
      function(subject, topic, data) {
        if (topic != 'cookie-changed') {
          return;
        }
        if (data == 'deleted' || data == 'added' || data == 'changed') {
          cookies.onCookieChanged_(
              subject.QueryInterface(Components.interfaces.nsICookie2), data);
        }
      });
  this.cookieChangeObserver_.register_();
}

/**
 * Unloads the cookies module during onUnload message from the browser.
 * @public
 */
CEEE_cookie_internal_.onUnload = function() {
  this.cookieChangeObserver_.unregister_();
};

/**
 * Returns a cookie store ID according to the current private browsing state.
 * @return Current cookie store ID.
 * @private
 */
CEEE_cookie_internal_.getCurrentStoreId_ = function() {
  return CEEE_globals.privateBrowsingService.isInPrivateBrowsing ?
         this.STORE_PRIVATE :
         this.STORE_NORMAL;
};


/**
 * @typedef {{
 *   name : string,
 *   value : string,
 *   domain : string,
 *   hostOnly : boolean,
 *   path : string,
 *   secure : boolean,
 *   httpOnly : boolean,
 *   session : boolean,
 *   expirationDate: ?number,
 *   storeId : string
 * }}
 */
CEEE_cookie_internal_.Cookie;

/**
 * Builds an object that represents a "cookie", as defined by the Google Chrome
 * extension API.
 * @param {nsICookie2} cookie Mozilla cookie info.
 * @return {CEEE_cookie_internal_.Cookie} chrome.cookies.Cookie object.
 * @private
 */
CEEE_cookie_internal_.buildCookieValue_ = function(cookie) {
  var host = cookie.host;
  if (host && host[0] == '.') host = host.substr(1);
  var info = {
    name : cookie.name,
    value : cookie.value,
    domain : host,
    hostOnly : !cookie.isDomain,
    path : cookie.path,
    secure : cookie.isSecure,
    httpOnly : cookie.isHttpOnly,
    session : cookie.isSession,
    storeId : this.getCurrentStoreId_()
  };
  if (!cookie.isSession) {
    info.expirationDate = cookie.expires;
  }

  return info;
};

/**
 * @typedef {{
 *   id: string,
 *   tabIds: Array.<string>
 * }}
 */
CEEE_cookie_internal_.CookieStore;

/**
 * Builds an object that represents a "cookie store", as defined
 * by the Google Chrome extension API.
 * @return {CEEE_cookie_internal_.CookieStore}
 *     chrome.cookies.CookieStore object.
 * @private
 */
CEEE_cookie_internal_.buildCookieStoreValue_ = function() {
  var info = {};
  info.id = this.getCurrentStoreId_();
  info.tabIds = [];

  return info;
};

/**
 * Sends an event message to Chrome API when a cookie is set or removed.
 * @param {nsICookie2} cookie Changed/removed cookie.
 * @param {string} action One of the "deleted", "added" or "changed" strings.
 * @private
 */
CEEE_cookie_internal_.onCookieChanged_ = function(cookie, action) {
  // Send the event notification to ChromeFrame.
  var info = [{
    removed: action == 'deleted',
    cookie: this.buildCookieValue_(cookie)
  }];
  var msg = ['cookies.onChanged', CEEE_json.encode(info)];
  this.ceeeInstance_.getCfHelper().postMessage(
      CEEE_json.encode(msg),
      this.ceeeInstance_.getCfHelper().TARGET_EVENT_REQUEST);
};


/**
 * Implements the 'cookies.get' function in the Chrome extension API.
 * @param cmd Command.
 * @param data Arguments.
 * @return {?CEEE_cookie_internal_.Cookie} Cookie found or null.
 * @private
 */
CEEE_cookie_internal_.CMD_GET_COOKIE = 'cookies.get';
CEEE_cookie_internal_.getCookie_ = function(cmd, data) {
  var args = CEEE_json.decode(data.args);
  var details = args[0];
  var cookie = this.getCookieInternal_(details);
  if (cookie) {
    return this.buildCookieValue_(cookie);
  }
};

/**
 * Implements the main (internal) part of the 'cookies.get' function.
 * @param {{url:string, name:string, storeId:?string}} details
 *     Cookie filter to get.
 * @return {?nsICookie2} Mozilla cookie object or null if cookie doesn't exist.
 * @private
 */
CEEE_cookie_internal_.getCookieInternal_ = function(details) {
  var newDetails = { url: details.url, name: details.name };
  if (typeof(details.storeId) == 'string') {
    newDetails.storeId = details.storeId;
  }
  var cookies = this.getAllCookiesInternal_(newDetails);
  if (cookies.length == 0) {
    return null;
  } else {
    var signum = function(n) {
      return n > 0 ? 1 : n < 0 ? -1 : 0;
    };
    cookies.sort(function(a, b) {
      return 4 * signum(b.host.length - a.host.length) +
             2 * signum(b.path.length - a.path.length) +
             signum((a.creationTime || 0) - (b.creationTime || 0));
    });
    return cookies[0];
  };
};

/**
 * Implements the 'cookies.getAll' function in the Chrome extension API.
 * @param cmd Command.
 * @param data Arguments.
 * @return {Array.<CEEE_cookie_internal_.Cookie>} Array of the cookies found
 *     against parameters.
 * @private
 */
CEEE_cookie_internal_.CMD_GET_ALL_COOKIES = 'cookies.getAll';
CEEE_cookie_internal_.getAllCookies_ = function(cmd, data) {
  var args = CEEE_json.decode(data.args);
  var details = args[0];
  var self = this;
  return this.getAllCookiesInternal_(details).map(function(cookie, i, a) {
    return self.buildCookieValue_(cookie);
  });
};

/**
 * Implements the main (internal) part of the 'cookies.getAll' function.
 * @param {Object} details Search parameters.
 * @return {Array.<nsICookie2>} Array of the cookies found against parameters.
 * @private
 */
CEEE_cookie_internal_.getAllCookiesInternal_ = function(details) {
  var storeId = this.getCurrentStoreId_();
  if (typeof(details.storeId) == 'string' && details.storeId != storeId) {
    // We do have access to current cookie store only.
    return [];
  }
  var cme = CEEE_cookieManager.enumerator;
  var cookies = [];
  while (cme.hasMoreElements()) {
    var cookie = cme.getNext().QueryInterface(Components.interfaces.nsICookie2);
    cookies.push(cookie);
  }


  /**
   * Filters cookies array with a filtering function.
   * @param {string} prop Property to filter on.
   * @param {string} type Property type.
   * @param {function(Object, number, Array.<Object>)} func Filtering function.
   */
  var filterCookies = function(prop, type, func) {
    if (typeof(details[prop]) == type) {
      cookies = cookies.filter(func);
    }
  };

  /**
   * Checks whether a given domain is a subdomain of another one.
   * @param {string} domain Domain name.
   * @param {string} subdomain Subdomain name.
   * @return {boolean} True iff subdomain is a sub-domain of the domain.
   */
  var isSubDomain = function(domain, subdomain) {
    var domLen = domain.length;
    var subdomLen = subdomain.length;
    return domLen <= subdomLen &&
           subdomain.substr(subdomLen - domLen) == domain &&
           (domLen == subdomLen ||
            domain[0] == '.' ||
            subdomain[subdomLen - domLen - 1] == '.');
  };

  /**
   * Filters cookies array by a domain name.
   * @param {string} domain Domain name.
   */
  var filterCookiesByDomain = function(domain) {
    cookies = cookies.filter(function(cookie, i, a) {
      // Check whether the given string is equal or a subdomain
      // of the cookie domain.
      return isSubDomain(domain, cookie.host);
    });
  };

  /**
   * Filters cookies array by the URL host.
   * @param {string} host Host name.
   */
  var filterCookiesByHost = function(host) {
    cookies = cookies.filter(function(cookie, i, a) {
      // Check whether the cookie domain is equal or a subdomain
      // of the given URL host.
      return isSubDomain(cookie.host, host);
    });
  };

  // Filtering by argument details.
  filterCookies('name', 'string', function(cookie, i, a) {
    return cookie.name == details.name;
  });
  filterCookies('path', 'string', function(cookie, i, a) {
    return cookie.path == details.path;
  });
  filterCookies('secure', 'boolean', function(cookie, i, a) {
    return cookie.isSecure == details.secure;
  });
  filterCookies('session', 'boolean', function(cookie, i, a) {
    return cookie.isSession == details.session;
  });
  if (details.domain) {
    filterCookiesByDomain(details.domain);
  }
  if (typeof(details.url) == 'string') {
    var uri = null;
    try {
      uri = CEEE_ioService.newURI(details.url, null, null);
    } catch (e) {
      // uri left null if something happens.
    }
    if (uri) {
      filterCookiesByHost(uri.host);
      var path = uri.path || '/';
      cookies = cookies.filter(function(cookie, i, a) {
        // Any valid subpath should pass the filter.
        return path.indexOf(cookie.path) == 0 &&
            (path.length == cookie.path.length ||
             path.charAt(cookie.path.length) == '/');
      });
    } else {
      // not valid URL; no cookie can match it.
      cookies = [];
    }
  }

  return cookies;
};

/**
 * Implements the 'cookies.remove' function in the Chrome extension API.
 * @param cmd Command.
 * @param data Arguments.
 * @private
 */
CEEE_cookie_internal_.CMD_REMOVE_COOKIE = 'cookies.remove';
CEEE_cookie_internal_.removeCookie_ = function(cmd, data) {
  var args = CEEE_json.decode(data.args);
  var details = args[0];
  var newDetails = { url: details.url, name: details.name };
  if (typeof(details.storeId) == 'string') {
    newDetails.storeId = details.storeId;
  }
  var cookie = this.getCookieInternal_(newDetails);
  if (cookie) {
    CEEE_cookieManager.remove(cookie.host, cookie.name, cookie.path, false);
  }
};

/**
 * Implements the 'cookies.set' function in the Chrome extension API.
 * @param cmd Command.
 * @param data Arguments.
 * @private
 */
CEEE_cookie_internal_.CMD_SET_COOKIE = 'cookies.set';
CEEE_cookie_internal_.setCookie_ = function(cmd, data) {
  var args = CEEE_json.decode(data.args);
  var details = args[0];
  var uri = null;
  try {
    uri = CEEE_ioService.newURI(details.url, null, null);
  } catch (e) {
    // Invalid URL.
    return;
  }
  var DOOMSDAY = (new Date(2222, 0, 1)).getTime() / 1000;
  var domain = details.domain ? '.' + details.domain : uri.host;
  var path = details.path || uri.path || '/';
  var name = details.name || '';
  var value = details.value || '';
  var secure = details.secure || false;
  var isHttpOnly = !details.domain;
  var isSession = typeof(details.expirationDate) != 'number';
  // Expiration date should be set even for a session cookie!
  // Otherwise the cookie never appears even up to the end of the session.
  // Possibly Mozilla bug?
  var expire = isSession ? DOOMSDAY : details.expirationDate;
  CEEE_cookieManager.add(
      domain, path, name, value, secure, isHttpOnly, isSession, expire);
};

/**
 * Implements the 'cookies.getAllCookieStores' function in
 * the Chrome extension API.
 * The Firefox browser always contains exactly one cookie store.
 * @param cmd Command.
 * @param data Arguments.
 * @return {Array.<CEEE_cookie_internal_.CookieStore>} Array of the
 *     CookieStore objects.
 * @private
 */
CEEE_cookie_internal_.CMD_GET_ALL_COOKIE_STORES =
    'cookies.getAllCookieStores';
CEEE_cookie_internal_.getAllCookieStores_ = function(cmd, data) {
  var storeId = this.getCurrentStoreId_();
  var wm = CEEE_mozilla_windows.service;
  var be = wm.getEnumerator(CEEE_mozilla_windows.WINDOW_TYPE);
  var stores = [this.buildCookieStoreValue_()];
  var nTab = 0;
  while (be.hasMoreElements()) {
    var win = be.getNext();
    var tabs = win.gBrowser.tabContainer.childNodes;
    for (var iTab = 0; iTab < tabs.length; iTab++) {
      var tabId = CEEE_mozilla_tabs.getTabId(tabs[iTab]);
      stores[0].tabIds.push(tabId);
    }
  }
  return stores;
};

/**
  * Initialization routine for the CEEE Cookies API module.
  * @param {!Object} ceeeInstance Reference to the global ceee instance.
  * @return {Object} Reference to the cookies module.
  * @public
  */
function CEEE_initialize_cookies(ceeeInstance) {
  CEEE_cookie_internal_.ceeeInstance_ = ceeeInstance;
  var cookies = CEEE_cookie_internal_;

  // Register the extension handling functions with the ceee instance.
  ceeeInstance.registerExtensionHandler(cookies.CMD_GET_COOKIE,
                                        cookies,
                                        cookies.getCookie_);
  ceeeInstance.registerExtensionHandler(cookies.CMD_GET_ALL_COOKIES,
                                        cookies,
                                        cookies.getAllCookies_);
  ceeeInstance.registerExtensionHandler(cookies.CMD_REMOVE_COOKIE,
                                        cookies,
                                        cookies.removeCookie_);
  ceeeInstance.registerExtensionHandler(cookies.CMD_SET_COOKIE,
                                        cookies,
                                        cookies.setCookie_);
  ceeeInstance.registerExtensionHandler(cookies.CMD_GET_ALL_COOKIE_STORES,
                                        cookies,
                                        cookies.getAllCookieStores_);

  return cookies;
}

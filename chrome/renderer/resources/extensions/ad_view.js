// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shim that simulates a <adview> tag via Mutation Observers.
//
// The actual tag is implemented via the browser plugin. The internals of this
// are hidden via Shadow DOM.

// TODO(rpaquay): This file is currently very similar to "web_view.js". Do we
//                want to refactor to extract common pieces?

var adViewCustom = require('adViewCustom');
var chrome = requireNative('chrome').GetChrome();
var forEach = require('utils').forEach;
var watchForTag = require('tagWatcher').watchForTag;


var allowCustomAdNetworks = (function(allow){
  return function() { return Boolean(allow); }
})(adViewCustom ? adViewCustom.enabled : false);


// List of attribute names to "blindly" sync between <adview> tag and internal
// browser plugin.
var AD_VIEW_ATTRIBUTES = [
  'name',
];

// List of custom attributes (and their behavior)
//
// name: attribute name.
// onInit(adview): callback invoked when the <adview> element is created.
// onMutate(adview, mutation): callback invoked when attribute is mutated.
var AD_VIEW_CUSTOM_ATTRIBUTES = [
  {
    'name': "ad-network",
    'onInit': function(adview) {
      if (adview.node_.hasAttribute(this.name)) {
        var value = adview.node_.getAttribute(this.name);
        var item = getAdNetworkInfo(value);
        if (item) {
          adview.objectNode_.setAttribute("src", item.url);
        }
        else if (allowCustomAdNetworks()) {
          console.log('The ad-network \"' + value + '\" is not recognized, ' +
            'but custom ad-networks are enabled.');
        }
        else {
          console.error('The ad-network \"' + value + '\" is not recognized.');
        }
      }
    }
  },
  {
    'name': "src",
    'onInit': function(adview) {
      if (allowCustomAdNetworks()) {
        if (adview.node_.hasAttribute(this.name)) {
          var newValue = adview.node_.getAttribute(this.name);
          adview.objectNode_.setAttribute("src", newValue);
        }
      }
    },
    'onMutation': function(adview, mutation) {
      if (allowCustomAdNetworks()) {
        if (adview.node_.hasAttribute(this.name)) {
          var newValue = adview.node_.getAttribute(this.name);
          // Note: setAttribute does not work as intended here.
          //adview.objectNode_.setAttribute(this.name, newValue);
          adview.objectNode_[this.name] = newValue;
        }
        else {
          // If an attribute is removed from the BrowserPlugin, then remove it
          // from the <adview> as well.
          this.objectNode_.removeAttribute(this.name);
        }
      }
    }
  }
];

// List of api methods. These are forwarded to the browser plugin.
var AD_VIEW_API_METHODS = [
 // Empty for now.
];

// List of events to blindly forward from the browser plugin to the <adview>.
var AD_VIEW_EVENTS = {
  'loadcommit' : [],
  'sizechanged': ['oldHeight', 'oldWidth', 'newHeight', 'newWidth'],
};

// List of supported ad-networks.
//
// name: identifier of the ad-network, corresponding to a valid value
//       of the "ad-network" attribute of an <adview> element.
// url: url to navigate to when initially displaying the <adview>.
// origin: origin of urls the <adview> is allowed navigate to.
var AD_VIEW_AD_NETWORKS_WHITELIST = [
  {
    'name': 'admob',
    'url': 'https://admob-sdk.doubleclick.net/chromeapps',
    'origin': 'https://double.net'
  },
];

//
// Return the whitelisted ad-network entry named |name|.
//
function getAdNetworkInfo(name) {
  var result = null;
  forEach(AD_VIEW_AD_NETWORKS_WHITELIST, function(i, item) {
    if (item.name === name)
      result = item;
  });
  return result;
}

/**
 * @constructor
 */
function AdView(node) {
  this.node_ = node;
  var shadowRoot = node.webkitCreateShadowRoot();

  this.objectNode_ = document.createElement('object');
  this.objectNode_.type = 'application/browser-plugin';
  // The <object> node fills in the <adview> container.
  this.objectNode_.style.width = '100%';
  this.objectNode_.style.height = '100%';
  forEach(AD_VIEW_ATTRIBUTES, function(i, attributeName) {
    // Only copy attributes that have been assigned values, rather than copying
    // a series of undefined attributes to BrowserPlugin.
    if (this.node_.hasAttribute(attributeName)) {
      this.objectNode_.setAttribute(
        attributeName, this.node_.getAttribute(attributeName));
    }
  }, this);

  forEach(AD_VIEW_CUSTOM_ATTRIBUTES, function(i, attributeInfo) {
    if (attributeInfo.onInit) {
      attributeInfo.onInit(this);
    }
  }, this);

  shadowRoot.appendChild(this.objectNode_);

  // this.objectNode_[apiMethod] are not necessarily defined immediately after
  // the shadow object is appended to the shadow root.
  var self = this;
  forEach(AD_VIEW_API_METHODS, function(i, apiMethod) {
    node[apiMethod] = function(var_args) {
      return self.objectNode_[apiMethod].apply(self.objectNode_, arguments);
    };
  }, this);

  // Map attribute modifications on the <adview> tag to property changes in
  // the underlying <object> node.
  var handleMutation = function(i, mutation) {
    this.handleMutation_(mutation);
  }.bind(this);
  var observer = new WebKitMutationObserver(function(mutations) {
    forEach(mutations, handleMutation);
  });
  observer.observe(
      this.node_,
      {attributes: true, attributeFilter: AD_VIEW_ATTRIBUTES});

  var handleObjectMutation = function(i, mutation) {
    this.handleObjectMutation_(mutation);
  }.bind(this);
  var objectObserver = new WebKitMutationObserver(function(mutations) {
    forEach(mutations, handleObjectMutation);
  });
  objectObserver.observe(
      this.objectNode_,
      {attributes: true, attributeFilter: AD_VIEW_ATTRIBUTES});

  // Map custom attribute modifications on the <adview> tag to property changes
  // in the underlying <object> node.
  var handleCustomMutation = function(i, mutation) {
    this.handleCustomMutation_(mutation);
  }.bind(this);
  var observer = new WebKitMutationObserver(function(mutations) {
    forEach(mutations, handleCustomMutation);
  });
  var customAttributeNames =
    AD_VIEW_CUSTOM_ATTRIBUTES.map(function(item) { return item.name; });
  observer.observe(
      this.node_,
      {attributes: true, attributeFilter: customAttributeNames});

  var objectNode = this.objectNode_;
  // Expose getters and setters for the attributes.
  forEach(AD_VIEW_ATTRIBUTES, function(i, attributeName) {
    Object.defineProperty(this.node_, attributeName, {
      get: function() {
        return objectNode[attributeName];
      },
      set: function(value) {
        objectNode[attributeName] = value;
      },
      enumerable: true
    });
  }, this);

  // We cannot use {writable: true} property descriptor because we want dynamic
  // getter value.
  Object.defineProperty(this.node_, 'contentWindow', {
    get: function() {
      // TODO(fsamuel): This is a workaround to enable
      // contentWindow.postMessage until http://crbug.com/152006 is fixed.
      if (objectNode.contentWindow)
        return objectNode.contentWindow.self;
      console.error('contentWindow is not available at this time. ' +
          'It will become available when the page has finished loading.');
    },
    // No setter.
    enumerable: true
  });

  for (var eventName in AD_VIEW_EVENTS) {
    this.setupEvent_(eventName, AD_VIEW_EVENTS[eventName]);
  }
}

/**
 * @private
 */
AdView.prototype.handleMutation_ = function(mutation) {
  // This observer monitors mutations to attributes of the <adview> and
  // updates the BrowserPlugin properties accordingly. In turn, updating
  // a BrowserPlugin property will update the corresponding BrowserPlugin
  // attribute, if necessary. See BrowserPlugin::UpdateDOMAttribute for more
  // details.
  this.objectNode_[mutation.attributeName] =
      this.node_.getAttribute(mutation.attributeName);
};

/**
 * @private
 */
AdView.prototype.handleCustomMutation_ = function(mutation) {
  // This observer monitors mutations to attributes of the <adview> and
  // updates the BrowserPlugin properties accordingly. In turn, updating
  // a BrowserPlugin property will update the corresponding BrowserPlugin
  // attribute, if necessary. See BrowserPlugin::UpdateDOMAttribute for more
  // details.
  forEach(AD_VIEW_CUSTOM_ATTRIBUTES, function(i, item) {
    if (mutation.attributeName.toUpperCase() == item.name.toUpperCase()) {
      if (item.onMutation) {
        item.onMutation.bind(item)(this, mutation);
      }
    }
  }, this);
};

/**
 * @private
 */
AdView.prototype.handleObjectMutation_ = function(mutation) {
  // This observer monitors mutations to attributes of the BrowserPlugin and
  // updates the <adview> attributes accordingly.
  if (!this.objectNode_.hasAttribute(mutation.attributeName)) {
    // If an attribute is removed from the BrowserPlugin, then remove it
    // from the <adview> as well.
    this.node_.removeAttribute(mutation.attributeName);
  } else {
    // Update the <adview> attribute to match the BrowserPlugin attribute.
    // Note: Calling setAttribute on <adview> will trigger its mutation
    // observer which will then propagate that attribute to BrowserPlugin. In
    // cases where we permit assigning a BrowserPlugin attribute the same value
    // again (such as navigation when crashed), this could end up in an infinite
    // loop. Thus, we avoid this loop by only updating the <adview> attribute
    // if the BrowserPlugin attributes differs from it.
    var oldValue = this.node_.getAttribute(mutation.attributeName);
    var newValue = this.objectNode_.getAttribute(mutation.attributeName);
    if (newValue != oldValue) {
      this.node_.setAttribute(mutation.attributeName, newValue);
    }
  }
};

/**
 * @private
 */
AdView.prototype.setupEvent_ = function(eventname, attribs) {
  var node = this.node_;
  this.objectNode_.addEventListener('-internal-' + eventname, function(e) {
    var evt = new Event(eventname, { bubbles: true });
    var detail = e.detail ? JSON.parse(e.detail) : {};
    forEach(attribs, function(i, attribName) {
      evt[attribName] = detail[attribName];
    });
    node.dispatchEvent(evt);
  });
}

//
// Hook up <adview> tag creation in DOM.
//
var watchForTag = require("tagWatcher").watchForTag;

window.addEventListener('DOMContentLoaded', function() {
  watchForTag('ADVIEW', function(addedNode) { new AdView(addedNode); });
});

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scripts that are conceptually part of core.js, but have UIWebView-specific
// details/behaviors.

goog.provide('__crWeb.coreDynamic');

goog.require('__crWeb.common');
goog.require('__crWeb.message');

/**
 * Namespace for this module.
 */
__gCrWeb.core_dynamic = {};

/* Beginning of anonymous object. */
(function() {
  /**
   * Resets common.JSONStringify to a clean copy. This can be called to ensure
   * that its copy is not of an override injected earlier by the page. This
   * must be called after document.body is present.
   */
  var resetJsonStringify_ = function() {
    var frame = document.createElement('iframe');
    // crwebnull protocol returns NO immediately to reject the load attempt.
    // A new frame is still created with an independent window object.
    frame.src = 'crwebnull://';
    document.body.appendChild(frame);
    // One some occasions the contentWindow is not available, or the JSON object
    // is not available. It is not clear why, but this defense still has value
    // when it can be applied.
    if (frame.contentWindow && frame.contentWindow.JSON) {
      // Refresh original stringify object from new window reference (if
      // available) in case the originally retained version was not the native
      // version.
      __gCrWeb.common.JSONStringify = frame.contentWindow.JSON.stringify;
    }
    document.body.removeChild(frame);
  };

  /**
   * Adds UIWebView specific event listeners.
   */
  __gCrWeb.core_dynamic.addEventListeners = function() {
    window.addEventListener('unload', function(evt) {
      // In the case of a newly-created UIWebView, the URL starts as
      // about:blank, the window.unload event fires, and then the URL changes.
      // However, at this point the window object is *not* reset. After this,
      // when the page changes for any reason the window object *is* reset.
      // For this reason, we do not report the window.unload event from the
      // default page to the first URL.
      // This is sent as an immediate command because if the message arrives
      // after the page change has taken effect, false positive security errors
      // can occur.
      if (!document._defaultPage)
        __gCrWeb.message.invokeOnHostImmediate({'command': 'window.unload'});
    });
  };

  /**
   * Applies UIWebView specific document-level overrides. These overrides
   * require the document body to be present; therefore the method sets event
   * listeners and timers to retry upon document body load if document body is
   * not yet present. Returns false if on default page or document is not
   * present.
   */
  __gCrWeb.core_dynamic.documentInject = function() {
    // The default page gets the injections to the window object, but not the
    // document object. On the first occasion the page changes (from the default
    // about:blank) the window.unload event fires but the window object does not
    // actually reset. However by the time the DOMContentLoaded event fires the
    // document object will have been reset (to a non-default page).
    if (document && document['_defaultPage']) {
      window.addEventListener('DOMContentLoaded', __gCrWeb.core.documentInject);
      return false;
    }
    if (!document || !document.body) {
      // Either the document or document body is not yet available... retest in
      // 1 / 4 of a second.
      window.setTimeout(__gCrWeb.core.documentInject, 250);
      return false;
    }

    // Try to guarantee a clean copy of common.JSONStringify.
    resetJsonStringify_();

    // Flush the message queue and send document.present message, if load has
    // not been aborted.
    if (!document._cancelled) {
      if (__gCrWeb.message) {
        __gCrWeb.message.invokeQueues();
      }
      __gCrWeb.message.invokeOnHost({'command': 'document.present'});
    }

    // Add event listener for title changes.
    var lastSeenTitle = document.title;
    document.addEventListener('DOMSubtreeModified', function(evt) {
      if (document.title !== lastSeenTitle) {
        lastSeenTitle = document.title;
        __gCrWeb.message.invokeOnHost({'command': 'document.retitled'});
      }
    });

    return true;
  };

  /**
   * Notifies client and handles post-document load tasks when document has
   * finished loading.
   */
  __gCrWeb.core_dynamic.handleDocumentLoaded = function() {
    var invokeOnHost_ = __gCrWeb.message.invokeOnHost;
    var loaded_ = function() {
      invokeOnHost_({'command': 'document.loaded'});
      // Send the favicons to the browser.
      __gCrWeb.sendFaviconsToHost();
      // Add placeholders for plugin content.
      if (__gCrWeb.common.updatePluginPlaceholders())
        __gCrWeb.message.invokeOnHost({'command': 'addPluginPlaceholders'});
    };

    if (document.readyState === 'loaded' || document.readyState === 'complete')
      loaded_();
    else
      window.addEventListener('load', loaded_);
  }


  /**
   * Sends anchor.click message.
   */
  __gCrWeb.core_dynamic.handleInternalClickEvent = function(node) {
    __gCrWeb.message.invokeOnHost({'command': 'anchor.click',
                                   'href': node.href});
  }

  /**
   * Exits Fullscreen video by calling webkitExitFullScreen on every video
   * element.
   */
  __gCrWeb['exitFullscreenVideo'] = function() {
    var videos = document.getElementsByTagName('video');
    var videosLength = videos.length;
    for (var i = 0; i < videosLength; ++i) {
      videos[i].webkitExitFullScreen();
    }
  };
}());

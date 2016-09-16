// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file adheres to closure-compiler conventions in order to enable
// compilation with ADVANCED_OPTIMIZATIONS. See http://goo.gl/FwOgy
//
// Installs and runs the plugin placeholder function on the |__gCrWeb| object.

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected.
 */
__gCrWeb['plugin'] = {};

/* Beginning of anonymous object. */
(function() {

  /* Data-URL version of plugin_blocked_android.png. Served this way rather
   * than with an intercepted URL to avoid messing up https pages.
   */
  __gCrWeb['plugin'].imageData_ =
      'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACQAAAAkCAQAAABLCVATAAAD' +
      'aklEQVR4Xn2Wz2tcVRTHP/e+O28mMxONJKlF4kIkP4luXFgQuuxCBaG41IWrLupOXLur+A' +
      'e4cmV3LiS6qujSLgq2CIKQUqS2YnWsRkzGSTIz7zyHw+EdchnkcOd+7+OeT84578tMwmet' +
      'O1fkar1RRNAgUJuqbeEn/0RUcdS6UX7w0X54/93qw4V+m0IReBiizhAYpG52kfrO86+F9/' +
      'YXNnukHOTpc5SHgpiOu1cT623FBELeGvgTXfppOAjN3dCKm7GIkWiY4LsBnqBPpGqAgN/z' +
      'CDMMBsCWX+pwibd5hzdZZmLNOsxDm8VAzIkt1hX5NLucqgrZm3RlIC/XscKTNlAQpvncMi' +
      'tAnEM33D4nqgbcosBSPT3DRTJ3+Cx+4UfV3/CQniMQQ5g2WMJkoGKHNodUCBDpsYEQ2KGm' +
      'JBKIFPT4nYckB9ueaPxRscamWczco3qXLcR9wx4ndBsziqFSjaOCAWLm4kj0xhhSMVFli4' +
      'opyYuLlJ7s+/xTE6IgcVBthUuW6goHZDiA5IeCAnFEhkKVxxQh+pnoqSeMCEw4Uvt5kEHP' +
      'c8IyF3iJ5De1NYSAMOYvOtxgwBqv0wcE5rR4gcQGq9Sc5wt7bq2JtfYtI0Ys8mCmLhFg7q' +
      'w6XKRStUHJiMJmpC8vglqypAOU/MwRiw7KYGKqxZSKqE/iTKrQAwGxv5oU4ZbzGHCTf1QN' +
      'OTXbQhJ/gbxKjy85IPECHQSQ3EFUfM0+93iZgluM6LuzDUTJOXpc5jcWeDb3DjQrsMhj9t' +
      'TdPcAq8mtjjunyFEtN8ohfOWaVZR88Qd2WKK15a5zoRY8ZmRaNIZ/yCZ/P1u0zY+9TASjc' +
      'q04YMzBhqAAUBXf5iWcITGdql3aTtpIZVnxGYvSxj1VPXUB0EtHnxBoT6iwgeXEwQfwC69' +
      'xmROAcr5DwESxa3XLGW9G9AgPGVKahzzb/UvEcq81PwCl/MyDMrUgxQeMH7tNniQW6nPKA' +
      'e5TU3KUFjPmTRxyofUsFeFVQqyENBHDAYyodJhR0CFrnfaYECgvAjdogEwZCVySQaJ8Zeq' +
      'AL874rsy+2ofT1ev5fkSdmihwF0jpOra/kskTHkGMckkG9Gg7Xvw9XtifXOy/GEgCr7H/r' +
      'yepFOFy5fu1agI9XH71RbRWRrDmHOhrfLYrx9ndv3Wz98R+P7LgG2uyMvgAAAABJRU5Erk' +
      'Jggg==';

  /**
   * Returns the first <embed> child of the given node, if any.
   * @param {Node} node The node to check.
   * @return {Node} The first <embed> child, or null.
   * @private
   */
  __gCrWeb['plugin'].getEmbedChild_ = function(node) {
    if (node.hasChildNodes()) {
      for (var i = 0; i < node.childNodes.length; i++) {
        if (node.childNodes[i].nodeName === 'EMBED') {
          return node.childNodes[i];
        }
      }
    }
    return null;
  };

  /**
   * Returns the size for the given plugin element. For the common
   * pattern of an IE-specific <object> wrapping an all-other-browsers <embed>,
   * the object doesn't have real style info (most notably size), so this uses
   * the embed in that case.
   * @param {HTMLElement} plugin The <object> node to check.
   * @return {Object} The size (width and height) for the plugin element.
   * @private
   */
  __gCrWeb['plugin'].getPluginSize_ = function(plugin) {
    var style;
    // For the common pattern of an IE-specific <object> wrapping an
    // all-other-browsers <embed>, the object doesn't have real style info
    // (most notably size), so this uses the embed in that case.
    var embedChild = __gCrWeb['plugin'].getEmbedChild_(plugin);
    if (embedChild) {
      style = window.getComputedStyle(embedChild);
    } else {
      style = window.getComputedStyle(plugin);
    }

    var width = parseFloat(style.width);
    var height = parseFloat(style.height);
    if (plugin.tagName === 'APPLET') {
      // Size computation doesn't always work correctly with applets in
      // UIWebView, so use the attributes as fallbacks.
      if (isNaN(width)) {
        width = parseFloat(plugin.width);
      }
      if (isNaN(height)) {
        height = parseFloat(plugin.height);
      }
    }

    return {
      'width': width,
      'height': height
    };
  };

  /**
   * Checks whether an element is "significant". Whether a plugin is
   * "significant" is a heuristic that attempts to determine if it's a critical
   * visual element for the page (i.e., not invisible, or an incidental ad).
   * @param {HTMLElement} plugin The <object> node to check.
   * @return {boolean} Whether the node is significant.
   * @private
   */
  __gCrWeb['plugin'].isSignificantPlugin_ = function(plugin) {
    var windowWidth = window.innerWidth;
    var windowHeight = window.innerHeight;
    var pluginSize = __gCrWeb['plugin'].getPluginSize_(plugin);
    var pluginWidth = parseFloat(pluginSize.width);
    var pluginHeight = parseFloat(pluginSize.height);
    // A plugin must be at least |significantFraction| of one dimension of the
    // page, and a minimum size in the other dimension (to weed out banners and
    // tall side ads).
    var minSize = Math.min(200, windowWidth / 2, windowHeight / 2);
    var significantFraction = 0.5;
    return (pluginWidth > windowWidth * significantFraction &&
            pluginHeight > minSize) ||
           (pluginHeight > windowHeight * significantFraction &&
            pluginWidth > minSize);
  };

  /**
   * Walks the list of detected plugin elements, adding a placeholder to any
   * that are "significant" (see above).
   * @param {string} message The message to show in the placeholder.
   */
  __gCrWeb['plugin']['addPluginPlaceholders'] = function(message) {
    var i, plugins = __gCrWeb['placeholderTargetPlugins'];
    for (i = 0; i < plugins.length; i++) {
      var plugin = plugins[i];
      if (!__gCrWeb['plugin'].isSignificantPlugin_(plugin)) {
        continue;
      }

      var pluginSize = __gCrWeb['plugin'].getPluginSize_(plugin);
      var widthStyle = pluginSize.width + 'px';
      var heightStyle = pluginSize.height + 'px';

      // The outer wrapper is a div with relative positioning, as an anchor for
      // an inner absolute-position element, whose height is based on whether or
      // not there's an embed. If there is, then it's zero height, to avoid
      // affecting the layout of the (presumably-full-size) <embed> fallback. If
      // not, it's full-height to ensure the placeholder takes up the right
      // amount of space in the page layout. Width is full-width either way, to
      // avoid being affected by container alignment.
      var placeholder = document.createElement('div');
      placeholder.style.width = widthStyle;
      if (__gCrWeb['plugin'].getEmbedChild_(plugin)) {
        placeholder.style.height = '0';
      } else {
        placeholder.style.height = heightStyle;
      }
      placeholder.style.position = 'relative';

      // Inside is a full-plugin-size solid box.
      var placeholderBox = document.createElement('div');
      placeholderBox.style.position = 'absolute';
      placeholderBox.style.boxSizing = 'border-box';
      placeholderBox.style.width = widthStyle;
      placeholderBox.style.height = heightStyle;
      placeholderBox.style.border = '1px solid black';
      placeholderBox.style.backgroundColor = '#808080';
      placeholder.appendChild(placeholderBox);

      // Inside that is the plugin placeholder image, centered.
      var pluginImg = document.createElement('img');
      var imageSize = 36;
      pluginImg.width = imageSize;
      pluginImg.height = imageSize;
      pluginImg.style.position = 'absolute';
      // Center vertically and horizontally.
      var halfSize = imageSize / 2;
      pluginImg.style.top = '50%';
      pluginImg.style.marginTop = '-' + halfSize + 'px';
      pluginImg.style.left = '50%';
      pluginImg.style.marginLeft = '-' + halfSize + 'px';
      pluginImg.src = __gCrWeb['plugin'].imageData_;
      placeholderBox.appendChild(pluginImg);

      // And below that, the message.
      var label = document.createElement('p');
      label.style.width = widthStyle;
      label.style.height = '1.5em';
      label.style.position = 'absolute';
      // Position below the image.
      label.style.top = '50%';
      label.style.marginTop = imageSize + 'px';
      // Center horizontally.
      label.style.textAlign = 'center';
      label.textContent = message;
      placeholderBox.appendChild(label);

      plugin.insertBefore(placeholder, plugin.firstChild);
    }
  };
}());  // End of anonymous object

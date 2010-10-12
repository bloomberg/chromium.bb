// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Sends 'activate' DOMUI message.
 */
function sendAction(values) {
  chrome.send('action', values);
}

var NetworkMenuItem = cr.ui.define('div');

NetworkMenuItem.prototype = {
  __proto__: MenuItem.prototype,

  /**
   * Internal method to initiailze the MenuItem.
   * @private
   */
  initMenuItem_: function(leftIconWidth) {
    // *TODO: eliminate code duplication with menu.js
    // MenuItem.prototype.initMenuItem_(leftIconWidth);
    var attrs = this.attrs;
    this.className = 'menu-item ' + attrs.type;
    this.menu_.addHandlers(this);
    if (leftIconWidth > 0) {
      this.classList.add('left-icon');

      var url;
      if (attrs.type == 'radio') {
        url = attrs.checked ?
            this.menu_.config_.radioOnUrl :
            this.menu_.config_.radioOffUrl;
      } else if (attrs.icon) {
        url = attrs.icon;
      } else if (attrs.type == 'check' && attrs.checked) {
        url = this.menu_.config_.checkUrl;
      }
      if (url) {
        this.style.backgroundImage = 'url(' + url + ')';
      }
      // TODO(oshima): figure out how to update left padding in rule.
      // 4 is the padding on left side of icon.
      var padding =
          4 + leftIconWidth + this.menu_.config_.icon_to_label_padding;
      this.style.paddingLeft = padding + 'px';
    }
    var label = document.createElement('div');

    label.className = 'menu-label';

    //////// NetworkMenuItem specific code:
    // TODO: Handle specific types of network, connecting icon.
    if (attrs.status && attrs.status != 'unknown') {
      label.appendChild(document.createTextNode(attrs.label));
      if (attrs.status == 'connected') {
        // label.appendChild(document.createElement('p')).
        //     appendChild(document.createTextNode(attrs.message));
        label.appendChild(document.createElement('p')).
            appendChild(document.createTextNode(attrs.ip_address));
      } else if (attrs.status == 'connecting') {
        label.appendChild(document.createElement('p')).
            appendChild(document.createTextNode(attrs.message));
      } else {
        // error
        label.appendChild(document.createElement('p')).
            appendChild(document.createTextNode(attrs.message));
        // TODO: error icon, reconnect button
      }
      // TODO: need_passphrase
      // TODO: remembered
    } else {
      label.textContent = attrs.label;
    }
    //////// End NetworkMenuItem specifi code

    if (attrs.font) {
      label.style.font = attrs.font;
    }
    this.appendChild(label);

  },
};


var NetworkMenu = cr.ui.define('div');

NetworkMenu.prototype = {
  __proto__: Menu.prototype,

  createMenuItem: function(attrs) {
    if (attrs.type == 'command') {
      return new NetworkMenuItem();
    } else {
      return new MenuItem();
    }
  },
};

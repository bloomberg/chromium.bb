// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';


/**
 * DefaultActionDialog contains a message, a list box, an ok button, and a
 * cancel button.
 * This dialog should be used as action picker for file operations.
 */
cr.define('cr.filebrowser', function() {

  /**
   * Creates dialog in DOM tree.
   *
   * @param {HTMLElement} parentNode Node to be parent for this dialog.
   * @constructor
   * @extends {cr.ui.dialogs.BaseDialog}
   */
  function DefaultActionDialog(parentNode) {
    cr.ui.dialogs.BaseDialog.call(this, parentNode);

    this.frame_.id = 'default-action-dialog';

    this.list_ = new cr.ui.List();
    this.list_.id = 'default-actions-list';
    this.frame_.insertBefore(this.list_, this.text_.nextSibling);

    this.selectionModel_ = this.list_.selectionModel =
        new cr.ui.ListSingleSelectionModel();
    this.dataModel_ = this.list_.dataModel = new cr.ui.ArrayDataModel([]);

    // List has max-height defined at css, so that list grows automatically,
    // but doesn't exceed predefined size.
    this.list_.autoExpands = true;
    this.list_.activateItemAtIndex = this.activateItemAtIndex_.bind(this);

    this.initialFocusElement_ = this.list_;

    var self = this;

    // Binding stuff doesn't work with constructors, so we have to create
    // closure here.
    this.list_.itemConstructor = function(item) {
      return self.renderItem(item);
    };
  }

  DefaultActionDialog.prototype = {
    __proto__: cr.ui.dialogs.BaseDialog.prototype
  };

  /**
   * @override
   */
  DefaultActionDialog.prototype.onInputFocus = function() {
    this.list_.select();
  };

  /**
   * Renders item for list.
   * @param {Object} item Item to render.
   */
  DefaultActionDialog.prototype.renderItem = function(item) {
    var result = this.document_.createElement('li');

    var div = this.document_.createElement('div');
    div.textContent = item.label;

    if (item.iconType)
      div.setAttribute('file-type-icon', item.iconType);

    if (item.iconUrl)
      div.style.backgroundImage = 'url(' + item.iconUrl + ')';

    if (item.class)
      div.classList.add(item.class);

    result.appendChild(div);

    cr.defineProperty(result, 'lead', cr.PropertyKind.BOOL_ATTR);
    cr.defineProperty(result, 'selected', cr.PropertyKind.BOOL_ATTR);

    return result;
  };

  /**
   * Shows dialog.
   *
   * @param {string} title Title in dialog caption.
   * @param {string} message Message in dialog caption.
   * @param {Array.<Object>} items Items to render in the list.
   * @param {number} defaultIndex Item to select by default.
   * @param {function(Object=)} onOk OK callback with the selected item.
   * @param {function()=} opt_onCancel Cancel callback.
   * @param {function()=} opt_onShow Show callback.
   */
  DefaultActionDialog.prototype.show = function(title, message, items,
      defaultIndex, onOk, opt_onCancel, opt_onShow) {

    cr.ui.dialogs.BaseDialog.prototype.showWithTitle.apply(
        this, [title, message, onOk, opt_onCancel, opt_onShow]);

    if (!message) {
      this.text_.setAttribute('hidden', 'hidden');
    } else {
      this.text_.removeAttribute('hidden');
    }

    this.list_.startBatchUpdates();
    this.dataModel_.splice(0, this.dataModel_.length);
    for (var i = 0; i < items.length; i++) {
      this.dataModel_.push(items[i]);
    }
    this.selectionModel_.selectedIndex = defaultIndex;
    this.list_.endBatchUpdates();
  };

  /**
   * List activation handler. Closes dialog and calls 'ok' callback.
   * @param {number} index Activated index.
   */
  DefaultActionDialog.prototype.activateItemAtIndex_ = function(index) {
    this.hide();
    this.onOk_(this.dataModel_.item(index));
  };

  /**
   * Closes dialog and invokes callback with currently-selected item.
   * @override
   */
  DefaultActionDialog.prototype.onOkClick_ = function() {
    this.activateItemAtIndex_(this.selectionModel_.selectedIndex);
  };

  /**
   * @override
   */
  DefaultActionDialog.prototype.onContainerKeyDown_ = function(event) {
    // Handle Escape.
    if (event.keyCode == 27) {
      this.onCancelClick_(event);
      event.preventDefault();
    } else if (event.keyCode == 32 || event.keyCode == 13) {
      this.onOkClick_();
      event.preventDefault();
    }
  };

  return {DefaultActionDialog: DefaultActionDialog};
});

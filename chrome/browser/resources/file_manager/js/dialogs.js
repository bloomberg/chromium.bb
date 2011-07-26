// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui.dialogs', function() {

  function BaseDialog(parentNode) {
    this.parentNode_ = parentNode;
    this.document_ = parentNode.ownerDocument;
    this.initDom_();
  }

  /**
   * Default text for Ok and Cancel buttons.
   *
   * Clients should override these with localized labels.
   */
  BaseDialog.OK_LABEL = 'Ok';
  BaseDialog.CANCEL_LABEL = 'Cancel';

  BaseDialog.prototype.initDom_ = function() {
    var doc = this.document_;
    this.container_ = doc.createElement('div');
    this.container_.className = 'cr-dialog-container';

    this.frame_ = doc.createElement('div');
    this.frame_.className = 'cr-dialog-frame';
    this.container_.appendChild(this.frame_);

    this.text_ = doc.createElement('div');
    this.text_.className = 'cr-dialog-text';
    this.frame_.appendChild(this.text_);

    var buttons = doc.createElement('div');
    buttons.className = 'cr-dialog-buttons';
    this.frame_.appendChild(buttons);

    this.cancelButton_ = doc.createElement('button');
    this.cancelButton_.className = 'cr-dialog-cancel';
    this.cancelButton_.textContent = BaseDialog.CANCEL_LABEL;
    this.cancelButton_.addEventListener('click',
                                        this.onCancelClick_.bind(this));
    buttons.appendChild(this.cancelButton_);

    this.okButton_ = doc.createElement('button');
    this.okButton_.className = 'cr-dialog-ok';
    this.okButton_.textContent = BaseDialog.OK_LABEL;
    this.okButton_.addEventListener('click', this.onOkClick_.bind(this));
    buttons.appendChild(this.okButton_);
  };

  BaseDialog.prototype.onOk_ = null;
  BaseDialog.prototype.onCancel_ = null;

  BaseDialog.prototype.onOkClick_ = function(event) {
    this.hide();
    if (this.onOk_)
      this.onOk_();
  };

  BaseDialog.prototype.onCancelClick_ = function(event) {
    this.hide();
    if (this.onCancel_)
      this.onCancel_();
  };

  BaseDialog.prototype.setOkLabel = function(label) {
    this.okButton_.textContent = label;
  };

  BaseDialog.prototype.setCancelLabel = function(label) {
    this.cancelButton_.textContent = label;
  };

  BaseDialog.prototype.show = function(message, onOk, onCancel, onShow) {
    this.parentNode_.appendChild(this.container_);

    this.onOk_ = onOk;
    this.onCancel_ = onCancel;

    this.text_.textContent = message;

    var top = (this.document_.body.clientHeight -
               this.frame_.clientHeight) / 2;
    var left = (this.document_.body.clientWidth -
                this.frame_.clientWidth) / 2;

    this.frame_.style.top = (top - 50) + 'px';
    this.frame_.style.left = (left + 10) + 'px';

    var self = this;
    setTimeout(function () {
      self.frame_.style.top = top + 'px';
      self.frame_.style.left = left + 'px';
      self.frame_.style.webkitTransitionProperty = 'left, top';
      self.container_.style.opacity = '1';
    }, 0);
  };

  BaseDialog.prototype.hide = function(onHide) {
    this.container_.style.opacity = '0';
    this.frame_.style.top = (parseInt(this.frame_.style.top) + 50) + 'px';
    this.frame_.style.left = (parseInt(this.frame_.style.left) - 10) + 'px';

    var self = this;
    setTimeout(function() {
      // Wait until the transition is done before removing the dialog.
      self.parentNode_.removeChild(self.container_);
      self.frame_.style.webkitTransitionProperty = '';
      if (onHide)
        onHide();
    }, 500);
  };

  function AlertDialog(parentNode) {
    BaseDialog.apply(this, [parentNode]);
    this.cancelButton_.style.display = 'none';
  }

  AlertDialog.prototype = {__proto__: BaseDialog.prototype};

  AlertDialog.prototype.show = function(message, onOk, onShow) {
    return BaseDialog.prototype.show.apply(this, [message, onOk, onOk, onShow]);
  };

  function ConfirmDialog(parentNode) {
    BaseDialog.apply(this, [parentNode]);
  }

  ConfirmDialog.prototype = {__proto__: BaseDialog.prototype};

  function PromptDialog(parentNode) {
    BaseDialog.apply(this, [parentNode]);
    this.input_ = this.document_.createElement('input');
    this.input_.setAttribute('type', 'text');
    this.frame_.insertBefore(this.input_, this.text_.nextSibling);
  }

  PromptDialog.prototype = {__proto__: BaseDialog.prototype};

  PromptDialog.prototype.show = function(message, defaultValue, onOk, onCancel,
                                        onShow) {
    this.input_.value = defaultValue || '';
    return BaseDialog.prototype.show.apply(this, [message, onOk, onCancel,
                                                  onShow]);
  };

  PromptDialog.prototype.getValue = function() {
    return this.input_.value;
  };

  PromptDialog.prototype.onOkClick_ = function(event) {
    this.hide();
    if (this.onOk_)
      this.onOk_(this.getValue());
  };

  return {
    BaseDialog: BaseDialog,
    AlertDialog: AlertDialog,
    ConfirmDialog: ConfirmDialog,
    PromptDialog: PromptDialog
  };
});

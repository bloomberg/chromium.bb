// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

    Polymer('template-switch', {

      /** @override */
      activateChild: function(child, index, external) {
        if (this._activeContent) {
          this._activeContent.remove();
        }
        var template = this.children[index];
        if (!template) return;

        var model    = this.modelForChild(template, index);
        var fragment = template.createInstance(model, this.bindingDelegate);

        this._activeContent = document.createElement('div');
        this._activeContent.appendChild(fragment);
        this.appendChild(this._activeContent);
      },

      /**
       * @method modelForChild
       * @protected
       * @param {!HTMLElement} child The child element.
       * @param {number}       index The index of the child.
       * @return {!Object} The object to use as a template model.
       */
      modelForChild: function(child, index) {
        return this.templateInstance && this.templateInstance.model || {};
      },

    });
  

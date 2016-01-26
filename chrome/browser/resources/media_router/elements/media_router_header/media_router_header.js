// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This Polymer element is used as a header for the media router interface.
Polymer({
  is: 'media-router-header',

  properties: {
    /**
     * The name of the icon used as the back button. This is set once, when
     * the |this| is ready.
     * @private {string}
     */
    arrowDropIcon_: {
      type: String,
      value: '',
    },

    /**
     * Whether or not the arrow drop icon should be disabled.
     * @type {boolean}
     */
    arrowDropIconDisabled: {
      type: Boolean,
      value: false,
    },

    /**
     * The header text to show.
     * @type {string}
     */
    headingText: {
      type: String,
      value: '',
    },

    /**
     * The current view that this header should reflect.
     * @type {?media_router.MediaRouterView}
     */
    view: {
      type: String,
      value: null,
      observer: 'updateHeaderCursorStyle_',
    },

    /**
     * The text to show in the tooltip.
     * @type {string}
     */
    tooltip: {
      type: String,
      value: '',
    },
  },

  attached: function() {
    // isRTL() only works after i18n_template.js runs to set <html dir>.
    // Set the back button icon based on text direction.
    this.arrowDropIcon_ = isRTL() ? 'arrow-forward' : 'arrow-back';
  },

  /**
   * @param {?media_router.MediaRouterView} view The current view.
   * @return {string} The current arrow-drop-* icon to use.
   * @private
   */
  computeArrowDropIcon_: function(view) {
    return view == media_router.MediaRouterView.CAST_MODE_LIST ?
        'arrow-drop-up' : 'arrow-drop-down';
  },

  /**
   * @param {?media_router.MediaRouterView} view The current view.
   * @return {boolean} Whether or not the arrow drop icon should be hidden.
   * @private
   */
  computeArrowDropIconHidden_: function(view) {
    return view != media_router.MediaRouterView.SINK_LIST &&
        view != media_router.MediaRouterView.CAST_MODE_LIST;
  },

  /**
   * @param {?media_router.MediaRouterView} view The current view.
   * @return {boolean} Whether or not the back button should be hidden.
   * @private
   */
  computeBackButtonHidden_: function(view) {
    return view != media_router.MediaRouterView.ROUTE_DETAILS;
  },

  /**
   * Handles a click on the arrow button by firing an arrow-click event.
   *
   * @private
   */
  onHeaderOrArrowClick_: function() {
    if (this.view == media_router.MediaRouterView.SINK_LIST ||
        this.view == media_router.MediaRouterView.CAST_MODE_LIST) {
      this.fire('header-or-arrow-click');
    }
  },

  /**
   * Handles a click on the back button by firing a back-click event.
   *
   * @private
   */
  onBackButtonClick_: function() {
    this.fire('back-click');
  },

  /**
   * Handles a click on the close button by firing a close-button-click event.
   *
   * @private
   */
  onCloseButtonClick_: function() {
    this.fire('close-button-click');
  },

  /**
   * Updates the cursor style for the header text when the view changes. When
   * the drop arrow is also shown, the header text is also clickable.
   *
   * @param {?media_router.MediaRouterView} view The current view.
   * @private
   */
  updateHeaderCursorStyle_: function(view) {
    this.$$('#header-text').style.cursor =
        view == media_router.MediaRouterView.SINK_LIST ||
        view == media_router.MediaRouterView.CAST_MODE_LIST ?
            'pointer' : 'auto';
  },
});

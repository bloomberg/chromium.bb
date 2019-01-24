// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'chooser-exception-list' shows a list of chooser exceptions for a given
 * chooser type.
 */
Polymer({
  is: 'chooser-exception-list',

  behaviors: [
    I18nBehavior,
    ListPropertyUpdateBehavior,
    SiteSettingsBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Array of chooser exceptions to display in the widget.
     * @type {!Array<ChooserException>}
     */
    chooserExceptions: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * The string ID of the chooser type that this element is displaying data
     * for.
     * See site_settings/constants.js for possible values.
     * @type {!settings.ChooserType}
     */
    chooserType: {
      observer: 'chooserTypeChanged_',
      type: String,
      value: settings.ChooserType.NONE,
    },

    /** @private */
    emptyListMessage_: {
      type: String,
      value: '',
    },

    /** @private */
    tooltipText_: String,
  },

  /** @override */
  created: function() {
    this.browserProxy_ =
        settings.SiteSettingsPrefsBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'contentSettingChooserPermissionChanged',
        this.objectWithinChooserTypeChanged_.bind(this));
  },

  /**
   * Called when a chooser exception changes permission and updates the element
   * if |category| is equal to the settings category of this element.
   * @param {settings.ContentSettingsTypes} category The content settings type
   *     that represents this permission category.
   * @param {settings.ChooserType} chooserType The content settings type that
   *     represents the chooser data for this permission.
   * @private
   */
  objectWithinChooserTypeChanged_: function(category, chooserType) {
    if (category === this.category && chooserType === this.chooserType) {
      this.chooserTypeChanged_();
    }
  },

  /**
   * Configures the visibility of the widget and shows the list.
   * @private
   */
  chooserTypeChanged_: function() {
    if (this.chooserType == settings.ChooserType.NONE) {
      return;
    }

    // Set the message to display when the exception list is empty.
    switch (this.chooserType) {
      case settings.ChooserType.USB_DEVICES:
        this.emptyListMessage_ = this.i18n('noUsbDevicesFound');
      default:
        this.emptyListMessage_ = '';
    }

    this.populateList_();
  },

  /**
   * Returns true if there are any chooser exceptions for this chooser type.
   * @return {boolean}
   * @private
   */
  hasExceptions_: function() {
    return this.chooserExceptions.length > 0;
  },

  /**
   * Need to use a common tooltip since the tooltip in the entry is cut off from
   * the iron-list.
   * @param{!CustomEvent<!{target: HTMLElement, text: string}>} e
   * @private
   */
  onShowTooltip_: function(e) {
    this.tooltipText_ = e.detail.text;
    const target = e.detail.target;
    // paper-tooltip normally determines the target from the |for| property,
    // which is a selector. Here paper-tooltip is being reused by multiple
    // potential targets.
    this.$.tooltip.target = target;
    const hide = () => {
      this.$.tooltip.hide();
      target.removeEventListener('mouseleave', hide);
      target.removeEventListener('blur', hide);
      target.removeEventListener('tap', hide);
      this.$.tooltip.removeEventListener('mouseenter', hide);
    };
    target.addEventListener('mouseleave', hide);
    target.addEventListener('blur', hide);
    target.addEventListener('tap', hide);
    this.$.tooltip.addEventListener('mouseenter', hide);
    this.$.tooltip.show();
  },

  /**
   * Populate the chooser exception list for display.
   * @private
   */
  populateList_: function() {
    this.browserProxy_.getChooserExceptionList(this.chooserType)
        .then(exceptionList => this.processExceptions_(exceptionList));
  },

  /**
   * Process the chooser exception list returned from the native layer.
   * @param {!Array<RawChooserException>} exceptionList
   * @private
   */
  processExceptions_: function(exceptionList) {
    const exceptions = exceptionList.map(exception => {
      const sites = exception.sites.map(this.expandSiteException);
      return Object.assign(exception, {sites});
    });

    this.updateList('chooserExceptions', x => x.displayName, exceptions);
  },
});

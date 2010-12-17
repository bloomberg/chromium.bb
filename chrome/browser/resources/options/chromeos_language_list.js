// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.language', function() {
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const LanguageOptions = options.LanguageOptions;
  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;
  const ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

  /**
   * Creates a new language list.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.List}
   */
  var LanguageList = cr.ui.define('list');

  /**
   * Gets display name from the given language code.
   * @param {string} languageCode Language code (ex. "fr").
   */
  LanguageList.getDisplayNameFromLanguageCode = function(languageCode) {
    // Build the language code to display name dictionary at first time.
    if (!this.languageCodeToDisplayName_) {
      this.languageCodeToDisplayName_ = {};
      var languageList = templateData.languageList;
      for (var i = 0; i < languageList.length; i++) {
        var language = languageList[i];
        this.languageCodeToDisplayName_[language.code] = language.displayName;
      }
    }

    return this.languageCodeToDisplayName_[languageCode];
  }

  /**
   * Gets native display name from the given language code.
   * @param {string} languageCode Language code (ex. "fr").
   */
  LanguageList.getNativeDisplayNameFromLanguageCode = function(languageCode) {
    // Build the language code to display name dictionary at first time.
    if (!this.languageCodeToNativeDisplayName_) {
      this.languageCodeToNativeDisplayName_ = {};
      var languageList = templateData.languageList;
      for (var i = 0; i < languageList.length; i++) {
        var language = languageList[i];
        this.languageCodeToNativeDisplayName_[language.code] =
            language.nativeDisplayName;
      }
    }

    return this.languageCodeToNativeDisplayName_[languageCode];
  }

  /**
   * Returns true if the given language code is valid.
   * @param {string} languageCode Language code (ex. "fr").
   */
  LanguageList.isValidLanguageCode = function(languageCode) {
    // Having the display name for the language code means that the
    // language code is valid.
    if (LanguageList.getDisplayNameFromLanguageCode(languageCode)) {
      return true;
    }
    return false;
  }

  LanguageList.prototype = {
    __proto__: List.prototype,

    // The list item being dragged.
    draggedItem: null,
    // The drop position information: "below" or "above".
    dropPos: null,
    // The preference is a CSV string that describes preferred languages
    // in Chrome OS. The language list is used for showing the language
    // list in "Language and Input" options page.
    preferredLanguagesPref: 'settings.language.preferred_languages',
    // The preference is a CSV string that describes accept languages used
    // for content negotiation. To be more precise, the list will be used
    // in "Accept-Language" header in HTTP requests.
    acceptLanguagesPref: 'intl.accept_languages',

    /** @inheritDoc */
    decorate: function() {
      List.prototype.decorate.call(this);
      this.selectionModel = new ListSingleSelectionModel;

      // HACK(arv): http://crbug.com/40902
      window.addEventListener('resize', this.redraw.bind(this));

      // Listen to pref change.
      Preferences.getInstance().addEventListener(this.preferredLanguagesPref,
          this.handlePreferredLanguagesPrefChange_.bind(this));

      // Listen to drag and drop events.
      this.addEventListener('dragstart', this.handleDragStart_.bind(this));
      this.addEventListener('dragenter', this.handleDragEnter_.bind(this));
      this.addEventListener('dragover', this.handleDragOver_.bind(this));
      this.addEventListener('drop', this.handleDrop_.bind(this));
    },

    createItem: function(languageCode) {
      var languageDisplayName =
          LanguageList.getDisplayNameFromLanguageCode(languageCode);
      var languageNativeDisplayName =
          LanguageList.getNativeDisplayNameFromLanguageCode(languageCode);
      return new ListItem({
        label: languageDisplayName,
        draggable: true,
        languageCode: languageCode,
        title: languageNativeDisplayName  // Show native name as tooltip.
      });
    },

    /*
     * Adds a language to the language list.
     * @param {string} languageCode language code (ex. "fr").
     */
    addLanguage: function(languageCode) {
      // It shouldn't happen but ignore the language code if it's
      // null/undefined, or already present.
      if (!languageCode || this.dataModel.indexOf(languageCode) >= 0) {
        return;
      }
      this.dataModel.push(languageCode);
      // Select the last item, which is the language added.
      this.selectionModel.selectedIndex = this.dataModel.length - 1;

      this.savePreference_();
    },

    /*
     * Gets the language codes of the currently listed languages.
     */
    getLanguageCodes: function() {
      return this.dataModel.slice();
    },

    /*
     * Gets the language code of the selected language.
     */
    getSelectedLanguageCode: function() {
      return this.selectedItem;
    },

    /*
     * Selects the language by the given language code.
     * @returns {boolean} True if the operation is successful.
     */
    selectLanguageByCode: function(languageCode) {
      var index = this.dataModel.indexOf(languageCode);
      if (index >= 0) {
        this.selectionModel.selectedIndex = index;
        return true;
      }
      return false;
    },

    /*
     * Removes the currently selected language.
     */
    removeSelectedLanguage: function() {
      if (this.selectionModel.selectedIndex >= 0) {
        this.dataModel.splice(this.selectionModel.selectedIndex, 1);
        // Once the selected item is removed, there will be no selected item.
        // Select the item pointed by the lead index.
        this.selectionModel.selectedIndex = this.selectionModel.leadIndex;
        this.savePreference_();
      }
    },

    /*
     * Handles the dragstart event.
     * @param {Event} e The dragstart event.
     * @private
     */
    handleDragStart_: function(e) {
      var target = e.target;
      // ListItem should be the only draggable element type in the page,
      // but just in case.
      if (target instanceof ListItem) {
        this.draggedItem = target;
        e.dataTransfer.effectAllowed = 'move';
      }
    },

    /*
     * Handles the dragenter event.
     * @param {Event} e The dragenter event.
     * @private
     */
    handleDragEnter_: function(e) {
      e.preventDefault();
    },

    /*
     * Handles the dragover event.
     * @param {Event} e The dragover event.
     * @private
     */
    handleDragOver_: function(e) {
      var dropTarget = e.target;
      // Determins whether the drop target is to accept the drop.
      // The drop is only successful on another ListItem.
      if (!(dropTarget instanceof ListItem) ||
          dropTarget == this.draggedItem) {
        return;
      }
      // Compute the drop postion. Should we move the dragged item to
      // below or above the drop target?
      var rect = dropTarget.getBoundingClientRect();
      var dy = e.clientY - rect.top;
      var yRatio = dy / rect.height;
      var dropPos = yRatio <= .5 ? 'above' : 'below';
      this.dropPos = dropPos;
      e.preventDefault();
      // TODO(satorux): Show the drop marker just like the bookmark manager.
    },

    /*
     * Handles the drop event.
     * @param {Event} e The drop event.
     * @private
     */
    handleDrop_: function(e) {
      var dropTarget = e.target;

      // Delete the language from the original position.
      var languageCode = this.draggedItem.languageCode;
      var originalIndex = this.dataModel.indexOf(languageCode);
      this.dataModel.splice(originalIndex, 1);
      // Insert the language to the new position.
      var newIndex = this.dataModel.indexOf(dropTarget.languageCode);
      if (this.dropPos == 'below')
        newIndex += 1;
      this.dataModel.splice(newIndex, 0, languageCode);
      // The cursor should move to the moved item.
      this.selectionModel.selectedIndex = newIndex;
      // Save the preference.
      this.savePreference_();
    },

    /**
     * Handles preferred languages pref change.
     * @param {Event} e The change event object.
     * @private
     */
    handlePreferredLanguagesPrefChange_: function(e) {
      var languageCodesInCsv = e.value.value;
      var languageCodes = this.filterBadLanguageCodes_(
          languageCodesInCsv.split(','));
      this.load_(languageCodes);
    },

    /**
     * Loads given language list.
     * @param {Array} languageCodes List of language codes.
     * @private
     */
    load_: function(languageCodes) {
      // Preserve the original selected index. See comments below.
      var originalSelectedIndex = (this.selectionModel ?
                                   this.selectionModel.selectedIndex : -1);
      this.dataModel = new ArrayDataModel(languageCodes);
      if (originalSelectedIndex >= 0 &&
          originalSelectedIndex < this.dataModel.length) {
        // Restore the original selected index if the selected index is
        // valid after the data model is loaded. This is neeeded to keep
        // the selected language after the languge is added or removed.
        this.selectionModel.selectedIndex = originalSelectedIndex;
        // The lead index should be updated too.
        this.selectionModel.leadIndex = originalSelectedIndex;
      } else if (this.dataModel.length > 0){
        // Otherwise, select the first item if it's not empty.
        // Note that ListSingleSelectionModel won't select an item
        // automatically, hence we manually select the first item here.
        this.selectionModel.selectedIndex = 0;
      }
    },

    /**
     * Saves the preference.
     */
    savePreference_: function() {
      // Encode the language codes into a CSV string.
      Preferences.setStringPref(this.preferredLanguagesPref,
                                this.dataModel.slice().join(','));
      // Save the same language list as accept languages preference as
      // well, but we need to expand the language list, to make it more
      // acceptable. For instance, some web sites don't understand 'en-US'
      // but 'en'. See crosbug.com/9884.
      var acceptLanguages = this.expandLanguageCodes(this.dataModel.slice());
      Preferences.setStringPref(this.acceptLanguagesPref,
                                acceptLanguages.join(','));
      cr.dispatchSimpleEvent(this, 'save');
    },

    /**
     * Expands language codes to make these more suitable for Accept-Language.
     * Example: ['en-US', 'ja', 'en-CA'] => ['en-US', 'en', 'ja', 'en-CA'].
     * 'en' won't appear twice as this function eliminates duplicates.
     * @param {Array} languageCodes List of language codes.
     * @private
     */
    expandLanguageCodes: function(languageCodes) {
      var expandedLanguageCodes = [];
      var seen = {};  // Used to eliminiate duplicates.
      for (var i = 0; i < languageCodes.length; i++) {
        var languageCode = languageCodes[i];
        if (!(languageCode in seen)) {
          expandedLanguageCodes.push(languageCode);
          seen[languageCode] = true;
        }
        var parts = languageCode.split('-');
        if (!(parts[0] in seen)) {
          expandedLanguageCodes.push(parts[0]);
          seen[parts[0]] = true;
        }
      }
      return expandedLanguageCodes;
    },

    /**
     * Filters bad language codes in case bad language codes are
     * stored in the preference. Removes duplicates as well.
     * @param {Array} languageCodes List of language codes.
     * @private
     */
    filterBadLanguageCodes_: function(languageCodes) {
      var filteredLanguageCodes = [];
      var seen = {};
      for (var i = 0; i < languageCodes.length; i++) {
        // Check if the the language code is valid, and not
        // duplicate. Otherwise, skip it.
        if (LanguageList.isValidLanguageCode(languageCodes[i]) &&
            !(languageCodes[i] in seen)) {
          filteredLanguageCodes.push(languageCodes[i]);
          seen[languageCodes[i]] = true;
        }
      }
      return filteredLanguageCodes;
    },
  };

  return {
    LanguageList: LanguageList
  };
});

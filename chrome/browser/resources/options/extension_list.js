// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /**
   * Creates a new list of extensions.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.div}
   */
  var ExtensionsList = cr.ui.define('div');

  var handleInstalled = false;

  ExtensionsList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      this.initControlsAndHandlers_();

      var showingDetails = [];
      var showingWarning = [];
      this.deleteExistingExtensionNodes_(showingDetails, showingWarning);

      this.showExtensionNodes_(showingDetails, showingWarning);
    },

    /**
     * Initializes the controls (toggle section and button) and installs
     * handlers.
     * @private
     */
    initControlsAndHandlers_: function() {
      // Make sure developer mode section is set correctly as per saved setting.
      var toggleButton = $('toggle-dev-on');
      var toggleSection = $('dev');
      if (this.data_.developerMode) {
        toggleSection.classList.add('dev-open');
        toggleSection.classList.remove('dev-closed');
        toggleButton.checked = true;
      } else {
        toggleSection.classList.remove('dev-open');
        toggleSection.classList.add('dev-closed');
      }

      // Install handler for key presses.
      if (!handleInstalled) {
        document.addEventListener('keyup', this.upEventHandler_.bind(this));
        document.addEventListener('mouseup', this.upEventHandler_.bind(this));
        handleInstalled = true;
      }
    },

    /**
     * Deletes the existing Extension nodes from the page to make room for new
     * ones. It also keeps track of who was showing details so when the
     * extension list gets recreated we can recreate that state.
     * @param {Array} showingDetails An array that will contain the list of id's
     *                of extension that had the details section expanded.
     * @param {Array} showingWarning An array that will contain the list of id's
     *                of extension that were showing a warning.
     * @private
     */
     deleteExistingExtensionNodes_: function(showingDetails, showingWarning) {
      // Delete all child nodes before adding them back and while we are at it
      // make a note of which ones were in expanded state (and which showing
      // the warning) so we can restore those to the same state afterwards.
      while (this.hasChildNodes()){
        var child = this.firstChild;

        // See if the item is expanded.
        if (child.classList.contains('extension-list-item-expanded'))
          showingDetails.push(child.id);

        // See if the butterbar is showing.
        var butterBar = document.getElementById(child.id + '_incognitoWarning');
        if (!(butterBar === null) && !butterBar.hidden)
          showingWarning.push(child.id);

        // Now we can delete it.
        this.removeChild(child);
      }
    },

    /**
     * Handles decorating the details section.
     * @param {Element} details The div that the details should be attached to.
     * @param {Object} extension The extension we are shoting the details for.
     * @param {boolean} expanded Whether to show the details expanded or not.
     * @param {boolean} showButterbar Whether to show the incognito warning or
     *                  not.
     * @private
     */
     showExtensionNodes_: function(showingDetails, showingWarning) {
      // Keeps track of differences in checkbox width.
      var minCheckboxWidth = 999999;
      var maxCheckboxWidth = 0;

      // Iterate over the extension data and add each item to the list.
      for (var i = 0; i < this.data_.extensions.length; ++i) {
        var extension = this.data_.extensions[i];
        var id = extension.id;

        var wrapper = this.ownerDocument.createElement('div');

        // Figure out if the item should open expanded or not based on the state
        // of things before we deleted the items.
        var iter = showingDetails.length;
        var expanded = false;
        while (iter--) {
          if (showingDetails[iter] == id) {
            expanded = true;
            break;
          }
        }
        // Figure out if the butterbar should be showing.
        iter = showingWarning.length;
        var butterbar = false;
        while (iter--) {
          if (showingWarning[iter] == id) {
            butterbar = true;
            break;
          }
        }

        wrapper.classList.add(expanded ? 'extension-list-item-expanded' :
                                         'extension-list-item-collaped');
        if (!extension.enabled)
          wrapper.classList.add('disabled');
        wrapper.id = id;
        this.appendChild(wrapper);

        var vbox_outer = this.ownerDocument.createElement('div');
        vbox_outer.classList.add('vbox');
        vbox_outer.classList.add('extension-list-item');
        wrapper.appendChild(vbox_outer);

        var hbox = this.ownerDocument.createElement('div');
        hbox.classList.add('hbox');
        vbox_outer.appendChild(hbox);

        // Add a container div for the zippy, so we can extend the hit area.
        var container = this.ownerDocument.createElement('div');
        // Clicking anywhere on the div expands/collapses the details.
        container.classList.add('extension-zippy-container');
        container.addEventListener('click', this.handleZippyClick_.bind(this));
        hbox.appendChild(container);

        // On the far left we have the zippy icon.
        div = this.ownerDocument.createElement('div');
        div.id = id + '_zippy';
        div.classList.add('extension-zippy-default');
        div.classList.add(expanded ? 'extension-zippy-expanded' :
                                     'extension-zippy-collapsed');
        container.appendChild(div);

        // Next to it, we have the extension icon.
        icon = this.ownerDocument.createElement('img');
        icon.classList.add('extension-icon');
        icon.src = extension.icon;
        hbox.appendChild(icon);

        // Start a vertical box for showing the details.
        var vbox = this.ownerDocument.createElement('div');
        vbox.classList.add('vbox');
        vbox.classList.add('stretch');
        vbox.classList.add('details-view');
        hbox.appendChild(vbox);

        div = this.ownerDocument.createElement('div');
        vbox.appendChild(div);

        // Title comes next.
        var title = this.ownerDocument.createElement('span');
        title.classList.add('extension-title');
        title.textContent = extension.name;
        vbox.appendChild(title);

        // Followed by version.
        var version = this.ownerDocument.createElement('span');
        version.classList.add('extension-version');
        version.textContent = extension.version;
        vbox.appendChild(version);

        div = this.ownerDocument.createElement('div');
        vbox.appendChild(div);

        // And below that we have description (if provided).
        if (extension.description.length > 0) {
          var description = this.ownerDocument.createElement('span');
          description.classList.add('extension-description');
          description.textContent = extension.description;
          vbox.appendChild(description);
        }

        // Immediately following the description, we have the
        // Options link (optional).
        if (extension.options_url) {
          var link = this.ownerDocument.createElement('a');
          link.classList.add('extension-links-trailing');
          link.textContent = localStrings.getString('extensionSettingsOptions');
          link.href = '#';
          link.addEventListener('click', this.handleOptions_.bind(this));
          vbox.appendChild(link);
        }

        // Then the optional Visit Website link.
        if (extension.homepageUrl) {
          var link = this.ownerDocument.createElement('a');
          link.classList.add('extension-links-trailing');
          link.textContent =
            localStrings.getString('extensionSettingsVisitWebsite');
          link.href = '#';
          link.addEventListener('click', this.handleVisitWebsite_.bind(this));
          vbox.appendChild(link);
        }

        // And now the details section that is normally hidden.
        var details = this.ownerDocument.createElement('div');
        details.classList.add('vbox');
        vbox.appendChild(details);

        this.decorateDetailsSection_(details, extension, expanded, butterbar);

        // And on the right of the details we have the Enable/Enabled checkbox.
        div = this.ownerDocument.createElement('div');
        hbox.appendChild(div);

        var section = this.ownerDocument.createElement('section');
        section.classList.add('extension-enabling');
        div.appendChild(section);

        // The Enable checkbox.
        var input = this.ownerDocument.createElement('input');
        input.addEventListener('click', this.handleEnable_.bind(this));
        input.type = 'checkbox';
        input.name = 'toggle-' + id;
        if (!extension.mayDisable)
          input.disabled = true;
        if (extension.enabled)
          input.checked = true;
        input.id = 'toggle-' + id;
        section.appendChild(input);
        var label = this.ownerDocument.createElement('label');
        label.classList.add('extension-enabling-label');
        if (extension.enabled)
          label.classList.add('extension-enabling-label-bold');
        label.setAttribute('for', 'toggle-' + id);
        label.id = 'toggle-' + id + '-label';
        if (extension.enabled) {
          // Enabled (with a d).
          label.textContent =
              localStrings.getString('extensionSettingsEnabled');
        } else {
          // Enable (no d).
          label.textContent = localStrings.getString('extensionSettingsEnable');
        }
        section.appendChild(label);

        if (label.offsetWidth > maxCheckboxWidth)
          maxCheckboxWidth = label.offsetWidth;
        if (label.offsetWidth < minCheckboxWidth)
          minCheckboxWidth = label.offsetWidth;

        // And, on the far right we have the uninstall button.
        var button = this.ownerDocument.createElement('button');
        button.classList.add('extension-delete');
        button.id = id;
        if (!extension.mayDisable)
          button.disabled = true;
        button.textContent = localStrings.getString('extensionSettingsRemove');
        button.addEventListener('click', this.handleUninstall_.bind(this));
        hbox.appendChild(button);
      }

      // Do another pass, making sure checkboxes line up.
      var difference = maxCheckboxWidth - minCheckboxWidth;
      for (var i = 0; i < this.data_.extensions.length; ++i) {
        var extension = this.data_.extensions[i];
        var id = extension.id;
        var label = $('toggle-' + id + '-label');
        if (label.offsetWidth < maxCheckboxWidth)
          label.style.marginRight = difference.toString() + 'px';
      }
    },

    /**
     * Handles decorating the details section.
     * @param {Element} details The div that the details should be attached to.
     * @param {Object} extension The extension we are shoting the details for.
     * @param {boolean} expanded Whether to show the details expanded or not.
     * @param {boolean} showButterbar Whether to show the incognito warning or
     *                  not.
     * @private
     */
    decorateDetailsSection_: function(details, extension,
                                      expanded, showButterbar) {
      // This container div is needed because vbox display
      // overrides display:hidden.
      var details_contents = this.ownerDocument.createElement('div');
      details_contents.classList.add(expanded ? 'extension-details-visible' :
                                                'extension-details-hidden');
      details_contents.id = extension.id + '_details';
      details.appendChild(details_contents);

      var div = this.ownerDocument.createElement('div');
      div.classList.add('informative-text');
      details_contents.appendChild(div);

      // Keep track of how many items we'll show in the details section.
      var itemsShown = 0;

      if (this.data_.developerMode) {
        // First we have the id.
        var content = this.ownerDocument.createElement('div');
        content.textContent =
          localStrings.getString('extensionSettingsExtensionId') +
                                 ' ' + extension.id;
        div.appendChild(content);
        itemsShown++;

        // Then, the path, if provided by unpacked extension.
        if (extension.isUnpacked) {
          content = this.ownerDocument.createElement('div');
          content.textContent =
              localStrings.getString('extensionSettingsExtensionPath') +
                                     ' ' + extension.path;
          div.appendChild(content);
          itemsShown++;
        }

        // Then, the 'managed, cannot uninstall/disable' message.
        if (!extension.mayDisable) {
          content = this.ownerDocument.createElement('div');
          content.textContent =
              localStrings.getString('extensionSettingsPolicyControlled');
          div.appendChild(content);
          itemsShown++;
        }

        // Then active views:
        if (extension.views.length > 0) {
          var table = this.ownerDocument.createElement('table');
          table.classList.add('extension-inspect-table');
          div.appendChild(table);
          var tr = this.ownerDocument.createElement('tr');
          table.appendChild(tr);
          var td = this.ownerDocument.createElement('td');
          td.classList.add('extension-inspect-left-column');
          tr.appendChild(td);
          var span = this.ownerDocument.createElement('span');
          td.appendChild(span);
          span.textContent =
              localStrings.getString('extensionSettingsInspectViews');

          td = this.ownerDocument.createElement('td');
          for (var i = 0; i < extension.views.length; ++i) {
            // Then active views:
            content = this.ownerDocument.createElement('div');
            var link = this.ownerDocument.createElement('a');
            link.classList.add('extension-links-view');
            link.textContent = extension.views[i].path;
            link.id = extension.id;
            link.href = '#';
            link.addEventListener('click', this.sendInspectMessage_.bind(this));
            content.appendChild(link);
            td.appendChild(content);
            tr.appendChild(td);

            itemsShown++;
          }
        }
      }

      var content = this.ownerDocument.createElement('div');
      details_contents.appendChild(content);

      // Then Reload:
      if (extension.enabled && extension.allow_reload) {
        var link = this.ownerDocument.createElement('a');
        link.classList.add('extension-links-trailing');
        link.textContent = localStrings.getString('extensionSettingsReload');
        link.id = extension.id;
        link.href = '#';
        link.addEventListener('click', this.handleReload_.bind(this));
        content.appendChild(link);
        itemsShown++;
      }

      // Then Show (Browser Action) Button:
      if (extension.enabled && extension.enable_show_button) {
        link = this.ownerDocument.createElement('a');
        link.classList.add('extension-links-trailing');
        link.textContent =
            localStrings.getString('extensionSettingsShowButton');
        link.id = extension.id;
        link.href = '#';
        link.addEventListener('click', this.handleShowButton_.bind(this));
        content.appendChild(link);
        itemsShown++;
      }

      if (extension.enabled && !extension.wantsFileAccess) {
        // The 'allow in incognito' checkbox.
        var label = this.ownerDocument.createElement('label');
        label.classList.add('extension-checkbox-label');
        content.appendChild(label);
        var input = this.ownerDocument.createElement('input');
        input.addEventListener('click',
                               this.handleToggleEnableIncognito_.bind(this));
        input.id = extension.id;
        input.type = 'checkbox';
        if (extension.enabledIncognito)
          input.checked = true;
        label.appendChild(input);
        var span = this.ownerDocument.createElement('span');
        span.classList.add('extension-checkbox-span');
        span.textContent =
            localStrings.getString('extensionSettingsEnableIncognito');
        label.appendChild(span);
        itemsShown++;
      }

      if (extension.enabled && !extension.wantsFileAccess) {
        // The 'allow access to file URLs' checkbox.
        label = this.ownerDocument.createElement('label');
        label.classList.add('extension-checkbox-label');
        content.appendChild(label);
        var input = this.ownerDocument.createElement('input');
        input.addEventListener('click',
                               this.handleToggleAllowFileUrls_.bind(this));
        input.id = extension.id;
        input.type = 'checkbox';
        if (extension.allowFileAccess)
          input.checked = true;
        label.appendChild(input);
        var span = this.ownerDocument.createElement('span');
        span.classList.add('extension-checkbox-span');
        span.textContent =
            localStrings.getString('extensionSettingsAllowFileAccess');
        label.appendChild(span);
        itemsShown++;
      }

      if (extension.enabled && !extension.is_hosted_app) {
        // And add a hidden warning message for allowInIncognito.
        content = this.ownerDocument.createElement('div');
        content.id = extension.id + '_incognitoWarning';
        content.classList.add('butter-bar');
        content.hidden = !showButterbar;
        details_contents.appendChild(content);

        var span = this.ownerDocument.createElement('span');
        span.innerHTML =
            localStrings.getString('extensionSettingsIncognitoWarning');
        content.appendChild(span);
        itemsShown++;
      }

      var zippy = extension.id + '_zippy';
      $(zippy).style.display = (itemsShown > 0) ? 'block' : 'none';
    },

    /**
     * A lookup helper function to find an extension based on an id.
     * @param {string} id The |id| of the extension to look up.
     * @private
     */
    getExtensionWithId_: function(id) {
      for (var i = 0; i < this.data_.extensions.length; ++i) {
        if (this.data_.extensions[i].id == id)
          return this.data_.extensions[i];
      }
      return null;
    },

    /**
     * A lookup helper function to find the first node that has an id (starting
     * at |node| and going up the parent chain.
     * @param {Element} node The node to start looking at.
     * @private
     */
    findIdNode_: function(node) {
      while (node.id.length == 0) {
        node = node.parentNode;
        if (!node)
          return null;
      }
      return node;
    },

    /**
     * Handles the mouseclick on the zippy icon (that expands and collapses the
     * details section).
     * @param {Event} e Change event.
     * @private
     */
    handleZippyClick_: function(e) {
      var node = this.findIdNode_(e.target.parentNode);
      var iter = this.firstChild;
      while (iter) {
        var zippy = $(iter.id + '_zippy');
        var details = $(iter.id + '_details');
        if (iter.id == node.id) {
          // Toggle visibility.
          if (iter.classList.contains('extension-list-item-expanded')) {
            // Hide yo kids! Hide yo wife!
            zippy.classList.remove('extension-zippy-expanded');
            zippy.classList.add('extension-zippy-collapsed');
            details.classList.remove('extension-details-visible');
            details.classList.add('extension-details-hidden');
            iter.classList.remove('extension-list-item-expanded');
            iter.classList.add('extension-list-item-collaped');

            // Hide yo incognito warning.
            var butterBar = this.ownerDocument.getElementById(
                iter.id + '_incognitoWarning');
            if (!(butterBar === null))
              butterBar.hidden = true;
          } else {
            // Show the contents.
            zippy.classList.remove('extension-zippy-collapsed');
            zippy.classList.add('extension-zippy-expanded');
            details.classList.remove('extension-details-hidden');
            details.classList.add('extension-details-visible');
            iter.classList.remove('extension-list-item-collaped');
            iter.classList.add('extension-list-item-expanded');
          }
        }
        iter = iter.nextSibling;
      }
    },

    /**
     * Handles the mouse-up and keyboard-up events. This is used to limit the
     * number of items to show in the list, when the user is searching for items
     * with the search box. Otherwise, if one match is found, the whole list of
     * extensions would be shown when we only want the matching items to be
     * found.
     * @param {Event} e Change event.
     * @private
     */
    upEventHandler_: function(e) {
      var searchString = $('search-field').value.toLowerCase();
      var child = this.firstChild;
      while (child){
        var extension = this.getExtensionWithId_(child.id);
        if (searchString.length == 0) {
          // Show all.
          child.classList.remove('search-suppress');
        } else {
          // If the search string does not appear within the text of the
          // extension, then hide it.
          if ((extension.name.toLowerCase().indexOf(searchString) < 0) &&
              (extension.version.toLowerCase().indexOf(searchString) < 0) &&
              (extension.description.toLowerCase().indexOf(searchString) < 0)) {
            // Hide yo extension!
            child.classList.add('search-suppress');
          } else {
            // Show yourself!
            child.classList.remove('search-suppress');
          }
        }
        child = child.nextSibling;
      }
    },

    /**
     * Handles the Reload Extension functionality.
     * @param {Event} e Change event.
     * @private
     */
    handleReload_: function(e) {
      var node = this.findIdNode_(e.target);
      chrome.send('extensionSettingsReload', [node.id]);
    },

    /**
     * Handles the Show (Browser Action) Button functionality.
     * @param {Event} e Change event.
     * @private
     */
    handleShowButton_: function(e) {
      var node = this.findIdNode_(e.target);
      chrome.send('extensionSettingsShowButton', [node.id]);
    },

    /**
     * Handles the Enable/Disable Extension functionality.
     * @param {Event} e Change event.
     * @private
     */
    handleEnable_: function(e) {
      var node = this.findIdNode_(e.target.parentNode);
      var extension = this.getExtensionWithId_(node.id);
      chrome.send('extensionSettingsEnable',
                  [node.id, extension.enabled ? 'false' : 'true']);
      chrome.send('extensionSettingsRequestExtensionsData');
    },

    /**
     * Handles the Uninstall Extension functionality.
     * @param {Event} e Change event.
     * @private
     */
    handleUninstall_: function(e) {
      var node = this.findIdNode_(e.target.parentNode);
      chrome.send('extensionSettingsUninstall', [node.id]);
      chrome.send('extensionSettingsRequestExtensionsData');
    },

    /**
     * Handles the View Options link.
     * @param {Event} e Change event.
     * @private
     */
    handleOptions_: function(e) {
      var node = this.findIdNode_(e.target.parentNode);
      var extension = this.getExtensionWithId_(node.id);
      chrome.send('extensionSettingsOptions', [extension.id]);
    },

    /**
     * Handles the Visit Extension Website link.
     * @param {Event} e Change event.
     * @private
     */
    handleVisitWebsite_: function(e) {
      var node = this.findIdNode_(e.target.parentNode);
      var extension = this.getExtensionWithId_(node.id);
      document.location = extension.homepageUrl;
    },

    /**
     * Handles the Enable Extension In Incognito functionality.
     * @param {Event} e Change event.
     * @private
     */
    handleToggleEnableIncognito_: function(e) {
      var node = this.findIdNode_(e.target);
      var butterBar = document.getElementById(node.id + '_incognitoWarning');
      butterBar.hidden = !e.target.checked;
      chrome.send('extensionSettingsEnableIncognito',
                  [node.id, String(e.target.checked)]);
    },

    /**
     * Handles the Allow On File URLs functionality.
     * @param {Event} e Change event.
     * @private
     */
    handleToggleAllowFileUrls_: function(e) {
      var node = this.findIdNode_(e.target);
      chrome.send('extensionSettingsAllowFileAccess',
                  [node.id, String(e.target.checked)]);
    },

    /**
     * Tell the C++ ExtensionDOMHandler to inspect the page detailed in
     * |viewData|.
     * @param {Event} e Change event.
     * @private
     */
    sendInspectMessage_: function(e) {
      var extension = this.getExtensionWithId_(e.srcElement.id);
      for (var i = 0; i < extension.views.length; ++i) {
        if (extension.views[i].path == e.srcElement.innerText) {
          // TODO(aa): This is ghetto, but WebUIBindings doesn't support sending
          // anything other than arrays of strings, and this is all going to get
          // replaced with V8 extensions soon anyway.
          chrome.send('extensionSettingsInspect', [
            String(extension.views[i].renderProcessId),
            String(extension.views[i].renderViewId)
          ]);
        }
      }
    },
  };

  return {
    ExtensionsList: ExtensionsList
  };
});
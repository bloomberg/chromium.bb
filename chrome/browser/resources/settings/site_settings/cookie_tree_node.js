// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{hasChildren: boolean,
 *            id: string,
 *            title: string,
 *            totalUsage: string,
 *            type: string}}
 */
var CookieDetails;

/**
 * @typedef {{title: string,
 *            id: string,
 *            data: CookieDetails}}
 */
var CookieDataItem;

/**
 * @typedef {{site: string,
 *            id: string,
 *            localData: string}}
 */
var CookieDataSummaryItem;

cr.define('settings', function() {
  'use strict';

  /**
   * @constructor
   */
  function CookieTreeNode(data) {
    /**
     * The data for this cookie node.
     * @private {CookieDetails}
     */
    this.data_ = data;

    /**
     * The child cookie nodes.
     * @private {!Array<!settings.CookieTreeNode>}
     */
    this.children_ = [];
  };

  CookieTreeNode.prototype = {
    /**
     * Converts a list of cookies and add them as CookieTreeNode children to
     * the given parent node.
     * @param {!settings.CookieTreeNode} parentNode The parent node to add
     *     children to.
     * @param {!Array<!CookieDetails>} newNodes The list containing the data to
     *     add.
     */
    addChildNodes: function(parentNode, newNodes) {
      var nodes = newNodes.map(function(x) {
        return new settings.CookieTreeNode(x);
      });
      parentNode.children_ = nodes;
    },

    /**
     * Looks up a parent node and adds a list of CookieTreeNodes to them.
     * @param {string} parentId The ID of the parent to add the nodes to.
     * @param {!settings.CookieTreeNode} startingNode The node to start with
     *     when looking for the parent node to add the children to.
     * @param {!Array<!CookieDetails>} newNodes The list containing the data to
         add.
     * @return {boolean} True if the parent node was found.
     */
    populateChildNodes: function(parentId, startingNode, newNodes) {
      for (var i = 0; i < startingNode.children_.length; ++i) {
        if (startingNode.children_[i].data_.id == parentId) {
          this.addChildNodes(startingNode.children_[i], newNodes);
          return true;
        }

        if (this.populateChildNodes(
            parentId, startingNode.children_[i], newNodes)) {
          return true;
        }
      }
      return false;
    },

    /**
     * Removes child nodes from a node with a given id.
     * @param {string} id The id of the parent node to delete from.
     * @param {number} firstChild The index of the first child to start deleting
     *     from.
     * @param {number} count The number of children to delete.
     */
    removeByParentId: function(id, firstChild, count) {
      var node = id == null ? this : this.fetchNodeById(id, true);
      node.children_.splice(firstChild, count);
    },

    /**
     * Returns an array of cookies from the current node within the cookie tree.
     * @return {!Array<!CookieDataItem>} The Cookie List.
     */
    getCookieList: function() {
      var list = [];

      for (var group of this.children_) {
        for (var cookie of group.children_) {
          list.push({title: cookie.data_.title,
                     id: cookie.data_.id,
                     data: cookie.data_});
        }
      }

      return list;
    },

    /**
     * Get a summary list of all sites and their stored data.
     * @return {!Array<!CookieDataSummaryItem>} The summary list.
     */
    getSummaryList: function() {
      var list = [];
      for (var i = 0; i < this.children_.length; ++i) {
        var siteEntry = this.children_[i];
        var title = siteEntry.data_.title;
        var id = siteEntry.data_.id;
        var description = '';

        for (var j = 0; j < siteEntry.children_.length; ++j) {
          var descriptionNode = siteEntry.children_[j];
          if (j > 0)
            description += ', ';

          // Some types, like quota, have no description nodes.
          var dataType = '';
          if (descriptionNode.data_.type != undefined)
            dataType = descriptionNode.data_.type;
          else
            dataType = descriptionNode.children_[0].data_.type;

          var category = '';
          if (dataType == 'cookie') {
            var cookieCount = descriptionNode.children_.length;
            if (cookieCount > 1)
              category = loadTimeData.getStringF('cookiePlural', cookieCount);
            else
              category = loadTimeData.getString('cookieSingular');
          } else if (dataType == 'database') {
            category = loadTimeData.getString('cookieDatabaseStorage');
          } else if (dataType == 'local_storage' || dataType == 'indexed_db') {
            category = loadTimeData.getString('cookieLocalStorage');
          } else if (dataType == 'app_cache') {
            category = loadTimeData.getString('cookieAppCache');
          } else if (dataType == 'file_system') {
            category = loadTimeData.getString('cookieFileSystem');
          } else if (dataType == 'quota') {
            category = descriptionNode.data_.totalUsage;
          } else if (dataType == 'channel_id') {
            category = loadTimeData.getString('cookieChannelId');
          } else if (dataType == 'service_worker') {
            category = loadTimeData.getString('cookieServiceWorker');
          } else if (dataType == 'cache_storage') {
            category = loadTimeData.getString('cookieCacheStorage');
          } else if (dataType == 'flash_lso') {
            category = loadTimeData.getString('cookieFlashLso');
          }

          description += category;
        }
        list.push({ site: title, id: id, localData: description });
      }
      list.sort(function(a, b) {
        return a.site.localeCompare(b.site);
      });
      return list;
    },

    /**
     * Fetch a CookieTreeNode by ID.
     * @param {string} id The ID to look up.
     * @param {boolean} recursive Whether to search the children also.
     * @return {settings.CookieTreeNode} The node found, if any.
     */
    fetchNodeById: function(id, recursive) {
      for (var i = 0; i < this.children_.length; ++i) {
        if (this.children_[i] == null)
          return null;
        if (this.children_[i].data_.id == id)
          return this.children_[i];
        if (recursive) {
          var node = this.children_[i].fetchNodeById(id, true);
          if (node != null)
            return node;
        }
      }
      return null;
    },

    /**
     * Add cookie data to a given HTML node.
     * @param {HTMLElement} root The node to add the data to.
     * @param {!settings.CookieTreeNode} item The data to add.
     */
    addCookieData: function(root, item) {
      var fields = cookieInfo[item.data_.type];
      for (var field of fields) {
        // Iterate through the keys found in |cookieInfo| for the given |type|
        // and see if those keys are present in the data. If so, display them
        // (in the order determined by |cookieInfo|).
        var key = field[0];
        if (item.data_[key].length > 0) {
          var label = loadTimeData.getString(field[1]);

          var header = document.createElement('div');
          header.appendChild(document.createTextNode(label));
          var content = document.createElement('div');
          content.appendChild(document.createTextNode(item.data_[key]));
          root.appendChild(header);
          root.appendChild(content);
        }
      }
    },
  };

  return {
    CookieTreeNode: CookieTreeNode,
  };
});

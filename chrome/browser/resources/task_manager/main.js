// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @constructor */
function TaskManager() { }

cr.addSingletonGetter(TaskManager);

/*
 * Default columns (column_id, label, width)
 * @const
 */
var DEFAULT_COLUMNS = [['title', 'Title', 300],
                       ['networkUsage', 'Network', 85],
                       ['cpuUsage', 'CPU', 80],
                       ['privateMemory', 'Memory', 80]];

TaskManager.prototype = {
  /**
   * Handle window close.
   * @public
   */
  onClose: function () {
    if (!this.disabled_) {
      this.disabled_ = true;
      this.disableTaskManager();
    }
  },

  /**
   * Closes taskmanager dialog.
   * After this function is called, onClose() will be called.
   * @public
   */
  close: function () {
    window.close();
  },

  /**
   * Sends commands to kill a process.
   * @public
   */
  killProcess: function () {
    var selectedIndexes = this.selectionModel_.selectedIndexes;
    chrome.send('killProcess', selectedIndexes);
  },

  /**
   * Sends command to kill a process.
   * @public
   */
  openAboutMemory: function () {
    chrome.send('openAboutMemory');
  },

  /**
   * Sends command to disable taskmanager model.
   * @public
   */
  disableTaskManager: function () {
    chrome.send('disableTaskManager');
  },

  /**
   * Sends command to enable taskmanager model.
   * @public
   */
  enableTaskManager: function () {
    chrome.send('enableTaskManager');
  },

  /**
   * Initializes taskmanager.
   * @public
   */
  initialize: function (dialogDom) {
    if (!dialogDom) {
      console.log('ERROR: dialogDom is not defined.');
      return;
    }

    this.initialized_ = true;
    this.enableTaskManager();

    this.dialogDom_ = dialogDom;
    this.document_ = dialogDom.ownerDocument;

    $('close-window').addEventListener('click', this.close.bind(this));
    $('kill-process').addEventListener('click', this.killProcess.bind(this));
    $('about-memory-link').addEventListener('click',
                                            this.openAboutMemory.bind(this));

    this.initColumnModel_();
    this.selectionModel_ = new cr.ui.ListSelectionModel();
    this.dataModel_ = new cr.ui.ArrayDataModel([]);

    // Initializes compare functions for column sort.
    var dm = this.dataModel_;
    dm.setCompareFunction('networkUsage', function(a, b) {
        var value_id = 'networkUsageValue';
        return dm.defaultValuesCompareFunction(a[value_id], b[value_id]);
    });
    dm.setCompareFunction('cpuUsage', function(a, b) {
        var value_id = 'cpuUsageValue';
        return dm.defaultValuesCompareFunction(a[value_id], b[value_id]);
    });
    dm.setCompareFunction('privateMemory', function(a, b) {
        var value_id = 'privateMemoryValue';
        return dm.defaultValuesCompareFunction(a[value_id], b[value_id]);
    });

    this.initTable_();
    this.table_.redraw();
  },

  initColumnModel_: function () {
    var table_columns = new Array();
    for (var i = 0; i < DEFAULT_COLUMNS.length; i++) {
      table_columns.push(new cr.ui.table.TableColumn(DEFAULT_COLUMNS[i][0],
                                                     DEFAULT_COLUMNS[i][1],
                                                     DEFAULT_COLUMNS[i][2]));
    }

    for (var i = 0; i < table_columns.length; i++) {
      table_columns[i].renderFunction = this.renderText_.bind(this);
    }

    this.columnModel_ = new cr.ui.table.TableColumnModel(table_columns);
  },

  initTable_: function () {
    if (!this.dataModel_ || !this.selectionModel_ || !this.columnModel_) {
      console.log('ERROR: some models are not defined.');
      return;
    }

    this.table_ = this.dialogDom_.querySelector('.detail-table');
    cr.ui.Table.decorate(this.table_);

    this.table_.dataModel = this.dataModel_;
    this.table_.selectionModel = this.selectionModel_;
    this.table_.columnModel = this.columnModel_;
  },

  renderText_: function(entry, columnId, table) {
    var label = this.document_.createElement('div');
    switch (columnId) {
      case 'processId':
        label.className = 'detail-process-id pid' + entry.processId;
        label.textContent = entry.processId;
        break;
      case 'title':
        var image_str = "url(" + entry.icon + ")";
        label.className = 'detail-title';
        label.style.backgroundImage = image_str;
        label.textContent = entry.title;
        break;
      case 'privateMemory':
        label.className = 'detail-private-memory pid' + entry.processId;
        label.textContent = entry.privateMemory;
        break;
      case 'cpuUsage':
        label.className = 'detail-cpu-usage';
        label.textContent = entry.cpuUsage;
        break;
      case 'networkUsage':
        label.className = 'detail-cpu-usage';
        label.textContent = entry.networkUsage;
        break;
      default:
        label.className = 'detail-unknown';
        label.textContent = '(unknown)'
    }
    return label;
  },

  onTaskChange: function (start, length, tasks) {
    var dm = this.dataModel_;
    var sm = this.selectionModel_;
    if (!dm || !sm)
      return;

    // Splice takes the to-be-spliced-in array as individual parameters,
    // rather than as an array, so we need to perform some acrobatics...
    var args = [].slice.call(tasks);
    args.unshift(start, length);

    var oldSelectedIndexes = sm.selectedIndexes;
    dm.splice.apply(dm, args);
    sm.selectedIndexes = oldSelectedIndexes;
  },

  onTaskAdd: function (start, length, tasks) {
    var dm = this.dataModel_;
    if (!dm)
      return;

    // Splice takes the to-be-spliced-in array as individual parameters,
    // rather than as an array, so we need to perform some acrobatics...
    var args = [].slice.call(tasks);
    args.unshift(start, 0);

    dm.splice.apply(dm, args);
  },

  onTaskRemove: function (start, length, tasks) {
    var dm = this.dataModel_;
    if (!dm)
      return;
    dm.splice(start, length);
  },
};

var taskmanager = TaskManager.getInstance();

function init() {
  taskmanager.initialize(document.body);
}

function onClose() {
  return taskmanager.onClose();
}

document.addEventListener('DOMContentLoaded', init);
document.addEventListener('Close', onClose);

function taskAdded(start, length, tasks) {
  // Sometimes this can get called too early.
  if (!taskmanager)
    return;
  taskmanager.onTaskAdd(start, length, tasks);
}

function taskChanged(start, length,tasks) {
  // Sometimes this can get called too early.
  if (!taskmanager)
    return;
  taskmanager.onTaskChange(start, length, tasks);
}

function taskRemoved(start, length) {
  // Sometimes this can get called too early.
  if (!taskmanager)
    return;
  taskmanager.onTaskRemove(start, length);
}


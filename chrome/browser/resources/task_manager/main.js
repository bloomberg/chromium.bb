// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @constructor */
function TaskManager() { }

cr.addSingletonGetter(TaskManager);

/*
 * Default columns (column_id, label_id, width, is_default)
 * @const
 */
var DEFAULT_COLUMNS = [['title', 'page_column', 300, true],
                       ['networkUsage', 'network_column', 85, true],
                       ['cpuUsage', 'cpu_column', 80, true],
                       ['privateMemory', 'private_memory_column', 80, true],
                       ['processId', 'process_id_column', 120, false]];

var localStrings = new LocalStrings();

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

    this.is_column_shown_ = [];
    for (var i = 0; i < DEFAULT_COLUMNS.length; i++) {
      this.is_column_shown_[i] = DEFAULT_COLUMNS[i][3];
    }

    this.localized_column_ = [];
    for (var i = 0; i < DEFAULT_COLUMNS.length; i++) {
      var column_label_id = DEFAULT_COLUMNS[i][1];
      var localized_label = localStrings.getString(column_label_id);
      // Falls back to raw column_label_id if localized string is not defined.
      if (localized_label == "")
        localized_label = column_label_id;

      this.localized_column_[i] = localized_label;
    }

    this.initColumnModel_();
    this.selectionModel_ = new cr.ui.ListSelectionModel();
    this.dataModel_ = new cr.ui.ArrayDataModel([]);

    // Initializes compare functions for column sort.
    var dm = this.dataModel_;
    dm.setCompareFunction('networkUsage', function(a, b) {
        var value_id = 'networkUsageValue';
        return dm.defaultValuesCompareFunction(a[value_id][0], b[value_id][0]);
    });
    dm.setCompareFunction('cpuUsage', function(a, b) {
        var value_id = 'cpuUsageValue';
        return dm.defaultValuesCompareFunction(a[value_id][0], b[value_id][0]);
    });
    dm.setCompareFunction('privateMemory', function(a, b) {
        var value_id = 'privateMemoryValue';
        return dm.defaultValuesCompareFunction(a[value_id][0], b[value_id][0]);
    });

    this.initTable_();
    this.initColumnMenu_();
    this.table_.redraw();
  },

  initColumnModel_: function () {
    var table_columns = new Array();
    for (var i = 0; i < DEFAULT_COLUMNS.length; i++) {
      if (!this.is_column_shown_[i])
        continue;

      var column = DEFAULT_COLUMNS[i];
      table_columns.push(new cr.ui.table.TableColumn(column[0],
                                                     this.localized_column_[i],
                                                     column[2]));
    }

    for (var i = 0; i < table_columns.length; i++) {
      table_columns[i].renderFunction = this.renderText_.bind(this);
    }

    this.columnModel_ = new cr.ui.table.TableColumnModel(table_columns);
  },

  initColumnMenu_: function () {
    this.column_menu_commands_ = [];

    this.commandsElement_ = this.document_.createElement('commands');
    this.document_.body.appendChild(this.commandsElement_);

    this.columnSelectContextMenu_ = this.document_.createElement('menu');
    for (var i = 0; i < DEFAULT_COLUMNS.length; i++) {
      var column = DEFAULT_COLUMNS[i];

      // Creates command element to receive event.
      var command = this.document_.createElement('command');
      command.id = 'columnContextMenu-' + column[0];
      cr.ui.Command.decorate(command);
      this.column_menu_commands_[command.id] = command;
      this.commandsElement_.appendChild(command);

      // Creates menuitem element.
      var item = this.document_.createElement('menuitem');
      item.command = command;
      command.menuitem = item;
      item.textContent = this.localized_column_[i];
      if (this.is_column_shown_[i])
        item.setAttributeNode(this.document_.createAttribute("checked"));
      this.columnSelectContextMenu_.appendChild(item);
    }

    this.document_.body.appendChild(this.columnSelectContextMenu_);
    cr.ui.Menu.decorate(this.columnSelectContextMenu_);

    cr.ui.contextMenuHandler.addContextMenuProperty(this.table_.header);
    this.table_.header.contextMenu = this.columnSelectContextMenu_;

    this.document_.addEventListener('command', this.onCommand_.bind(this));
    this.document_.addEventListener('canExecute',
                                    this.onCommandCanExecute_.bind(this));

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
    var container = this.document_.createElement('div');
    container.id = 'detail-container-' + columnId + '-pid' + entry.processId;
    container.className = 'detail-container-' + columnId;

    if (entry[columnId]) {
      for (var i = 0; i < entry[columnId].length; i++) {
        var label = document.createElement('div');
        if (columnId == 'title') {
          var image = this.document_.createElement('img');
          image.className = 'detail-title-image';
          image.src = entry['icon'][i];
          label.appendChild(image);
          var text = this.document_.createElement('div');
          text.className = 'detail-title-text';
          text.textContent = entry['title'][i];
          label.appendChild(text);
        } else {
          label.textContent = entry[columnId][i];
        }
        label.id = 'detail-' + columnId + '-pid' + entry.processId + '-' + i;
        label.className = 'detail-' + columnId + ' pid' + entry.processId;
        container.appendChild(label);
      }
    }
    return container;
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

  /**
   * Respond to a command being executed.
   */
  onCommand_: function(event) {
    var command = event.command;
    if (command.id.substr(0, 18) == 'columnContextMenu-') {
      console.log(command.id.substr(18));
      this.onColumnContextMenu_(command.id.substr(18), command);
    }
  },

  onCommandCanExecute_: function(event) {
    event.canExecute = true;
  },

  onColumnContextMenu_: function(id, command) {
    var menuitem = command.menuitem;
    var checked_item_count = 0;
    var is_uncheck = 0;

    // Leaves a item visible when user tries making invisible but it is the
    // last one.
    for (var i = 0; i < DEFAULT_COLUMNS.length; i++) {
      var column = DEFAULT_COLUMNS[i];
      if (column[0] == id && this.is_column_shown_[i]) {
        is_uncheck = 1;
      }
      if (this.is_column_shown_[i])
        checked_item_count++;
    }
    if (checked_item_count == 1 && is_uncheck) {
      return;
    }

    // Toggles the visibility of the column.
    for (var i = 0; i < DEFAULT_COLUMNS.length; i++) {
      var column = DEFAULT_COLUMNS[i];
      if (column[0] == id) {
        this.is_column_shown_[i] = !this.is_column_shown_[i];
        menuitem.checked = this.is_column_shown_[i];
        this.initColumnModel_()
        this.table_.columnModel = this.columnModel_;
        this.table_.redraw();
        return;
      }
    }
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


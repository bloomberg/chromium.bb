// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying encryption migration screen.
 */

/**
 * Enum for the UI states corresponding to sub steps inside migration screen.
 * These values must be kept in sync with
 * EncryptionMigrationScreenHandler::UIState in C++ code.
 * @enum {number}
 */
var EncryptionMigrationUIState = {
  INITIAL: 0,
  READY: 1,
  MIGRATING: 2,
  MIGRATION_SUCCEEDED: 3,
  MIGRATION_FAILED: 4,
  NOT_ENOUGH_SPACE: 5
};

Polymer({
  is: 'encryption-migration',

  properties: {
    /**
     * Current UI state which corresponds to a sub step in migration process.
     * @type {EncryptionMigrationUIState}
     */
    uiState: {
      type: Number,
      value: 0
    },

    /**
     * Current migration progress in range [0, 1]. Negative value means that
     * the progress is unknown.
     */
    progress: {
      type: Number,
      value: -1
    },

    /**
     * Whether the current migration is resuming the previous one.
     */
    isResuming: {
      type: Boolean,
      value: false
    },
  },

  /**
   * Returns true if the migration is in initial state.
   * @param {EncryptionMigrationUIState} state Current UI state
   * @private
   */
  isInitial_: function(state) {
    return state == EncryptionMigrationUIState.INITIAL;
  },

  /**
   * Returns true if the migration is ready to start.
   * @param {EncryptionMigrationUIState} state Current UI state
   * @private
   */
  isReady_: function(state) {
    return state == EncryptionMigrationUIState.READY;
  },

  /**
   * Returns true if the migration is in progress.
   * @param {EncryptionMigrationUIState} state Current UI state
   * @private
   */
  isMigrating_: function(state) {
    return state == EncryptionMigrationUIState.MIGRATING;
  },

  /**
   * Returns true if the migration is finished successfully.
   * @param {EncryptionMigrationUIState} state Current UI state
   * @private
   */
  isMigrationSucceeded_: function(state) {
    return state == EncryptionMigrationUIState.MIGRATION_SUCCEEDED;
  },

  /**
   * Returns true if the migration failed.
   * @param {EncryptionMigrationUIState} state Current UI state
   * @private
   */
  isMigrationFailed_: function(state) {
    return state == EncryptionMigrationUIState.MIGRATION_FAILED;
  },

  /**
   * Returns true if the available space is not enough to start migration.
   * @param {EncryptionMigrationUIState} state Current UI state
   * @private
   */
  isNotEnoughSpace_: function(state) {
    return state == EncryptionMigrationUIState.NOT_ENOUGH_SPACE;
  },

  /**
   * Returns true if the current migration progress is unknown.
   * @param {number} progress
   * @private
   */
  isProgressIndeterminate_: function(progress) {
    return progress < 0;
  },

  /**
   * Handles tap on UPGRADE button.
   * @private
   */
  onUpgrade_: function() {
    this.fire('upgrade');
  },

  /**
   * Handles tap on SKIP button.
   * @private
   */
  onSkip_: function() {
    this.fire('skip');
  },

  /**
   * Handles tap on RESTART button.
   * @private
   */
  onRestart_: function() {
    this.fire('restart');
  },
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /** @enum {string} */
  var ResolverState = {
    INITIAL: 'INITIAL',
    CONFIGURING_PRINTER: 'CONFIGURING_PRINTER',
    ERROR: 'ERROR',
    DONE: 'DONE'
  };

  /**
   * Overlay used to notify the user of printer setup steps for Chrome OS
   * printers.  Printer installation can occur before any print job is executed.
   *
   * @param {!print_preview.DestinationStore} destinationStore The destination
   *    store containing the destination. Used as a proxy to native layer for
   *    resolving the destination.
   * @param {!print_preview.Destination} destination The destination that has
   *     to be resolved.
   * @constructor
   * @extends {print_preview.Overlay}
   */
  function CrosDestinationResolver(destinationStore, destination) {
    print_preview.Overlay.call(this);

    /** @private {!print_preview.DestinationStore} */
    this.destinationStore_ = destinationStore;
    /** @private {!print_preview.Destination} */
    this.destination_ = destination;

    /** @private {!ResolverState} */
    this.state_ = ResolverState.INITIAL;

    /**
     * Promise resolver for promise returned by {@code this.run}.
     * @private {?PromiseResolver<!print_preview.Destination>}
     */
    this.promiseResolver_ = null;
  };

  CrosDestinationResolver.prototype = {
    __proto__: print_preview.Overlay.prototype,

    /** @override */
    enterDocument: function() {
      print_preview.Overlay.prototype.enterDocument.call(this);

      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.PRINTER_CONFIGURED,
          this.onDestinationResolved_.bind(this));
    },

    /** @override */
    createDom: function() {
      this.setElementInternal(this.cloneTemplateInternal(
          'cros-printer-resolver'));
    },

    /**
     * Dispatch the request to resolve the destination to the native layer.
     * @private
     */
    startResolveDestination_: function() {
      assert(this.state_ == ResolverState.INITIAL);
      assert(!this.promiseResolver_, 'Promise resolver already set');
      this.promiseResolver_ = new PromiseResolver();
      this.destinationStore_.resolveCrosDestination(this.destination_);
      this.setState_(ResolverState.CONFIGURING_PRINTER);
    },

    /**
     * Handler for PRINTER_SETUP event. It finalizes the
     * resolver state once the destination associated with the resolver gets
     * resolved.
     * @param {CustomEvent} event
     * @private
     */
    onDestinationResolved_: function(event) {
      assert(this.promiseResolver_, 'Promise resolver not found');
      var detail = event.detail;
      if (detail.success) {
        this.setState_(ResolverState.DONE);
        assert(detail.capabilities);
        // |detail| contains printerId and capabilities.
        this.promiseResolver_.resolve(detail);
        this.setIsVisible(false);
      } else {
        this.setState_(ResolverState.ERROR);
        this.promiseResolver_.reject();
      }

      this.promiseResolver_ = null;
    },

    /**
     * Sets new resolver state and updates the UI accordingly.
     * @param {!ResolverState} state
     * @private
     */
    setState_: function(state) {
      if (this.state_ == state)
        return;

      this.state_ = state;
      this.updateUI_();
    },

    /**
     * Updates the resolver overlay UI to match the resolver state.
     * @private
     */
    updateUI_: function() {
      // TODO(crbug.com/677567): Replace with meaningful UI.
    },

    /**
     * Initiates and shows the resolver overlay.
     * @param {!HTMLElement} parent The element that should parent the resolver
     *     UI.
     * @return {!Promise<!print_preview.Destination>} Promise that will be
     *     fulfilled when the destination resolving is finished.
     */
    run: function(parent) {
      this.render(parent);
      this.setIsVisible(true);
      this.startResolveDestination_();

      assert(this.promiseResolver_, 'Promise resolver not created.');
      return this.promiseResolver_.promise;
    }
  };

  return {
    CrosDestinationResolver: CrosDestinationResolver
  };
});

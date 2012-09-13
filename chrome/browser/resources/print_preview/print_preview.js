// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(rltoscano): Move data/* into print_preview.data namespace

var localStrings = new LocalStrings(templateData);

<include src="component.js"/>

cr.define('print_preview', function() {
  'use strict';

  /**
   * Container class for Chromium's print preview.
   * @constructor
   * @extends {print_preview.Component}
   */
  function PrintPreview() {
    print_preview.Component.call(this);

    /**
     * Used to communicate with Chromium's print system.
     * @type {!print_preview.NativeLayer}
     * @private
     */
    this.nativeLayer_ = new print_preview.NativeLayer();

    /**
     * Event target that contains information about the logged in user.
     * @type {!print_preview.UserInfo}
     * @private
     */
    this.userInfo_ = new print_preview.UserInfo();

    /**
     * Metrics object used to report usage statistics.
     * @type {!print_preview.Metrics}
     * @private
     */
    this.metrics_ = new print_preview.Metrics();

    /**
     * Application state.
     * @type {!print_preview.AppState}
     * @private
     */
    this.appState_ = new print_preview.AppState();

    /**
     * Data store which holds print destinations.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = new print_preview.DestinationStore(
        this.nativeLayer_, this.appState_);

    /**
     * Storage of the print ticket used to create the print job.
     * @type {!print_preview.PrintTicketStore}
     * @private
     */
    this.printTicketStore_ = new print_preview.PrintTicketStore(
        this.destinationStore_, this.appState_);

    /**
     * Holds the print and cancel buttons and renders some document statistics.
     * @type {!print_preview.PrintHeader}
     * @private
     */
    this.printHeader_ = new print_preview.PrintHeader(
        this.printTicketStore_, this.destinationStore_);
    this.addChild(this.printHeader_);

    /**
     * Component used to search for print destinations.
     * @type {!print_preview.DestinationSearch}
     * @private
     */
    this.destinationSearch_ = new print_preview.DestinationSearch(
        this.destinationStore_, this.userInfo_, this.metrics_);
    this.addChild(this.destinationSearch_);

    /**
     * Component that renders the print destination.
     * @type {!print_preview.DestinationSettings}
     * @private
     */
    this.destinationSettings_ = new print_preview.DestinationSettings(
        this.destinationStore_);
    this.addChild(this.destinationSettings_);

    /**
     * Component that renders UI for entering in page range.
     * @type {!print_preview.PageSettings}
     * @private
     */
    this.pageSettings_ = new print_preview.PageSettings(this.printTicketStore_);
    this.addChild(this.pageSettings_);

    /**
     * Component that renders the copies settings.
     * @type {!print_preview.CopiesSettings}
     * @private
     */
    this.copiesSettings_ = new print_preview.CopiesSettings(
        this.printTicketStore_);
    this.addChild(this.copiesSettings_);

    /**
     * Component that renders the layout settings.
     * @type {!print_preview.LayoutSettings}
     * @private
     */
    this.layoutSettings_ = new print_preview.LayoutSettings(
          this.printTicketStore_);
    this.addChild(this.layoutSettings_);

    /**
     * Component that renders the color options.
     * @type {!print_preview.ColorSettings}
     * @private
     */
    this.colorSettings_ = new print_preview.ColorSettings(
        this.printTicketStore_);
    this.addChild(this.colorSettings_);

    /**
     * Component that renders a select box for choosing margin settings.
     * @type {!print_preview.MarginSettings}
     * @private
     */
    this.marginSettings_ = new print_preview.MarginSettings(
        this.printTicketStore_);
    this.addChild(this.marginSettings_);

    /**
     * Component that renders miscellaneous print options.
     * @type {!print_preview.OtherOptionsSettings}
     * @private
     */
    this.otherOptionsSettings_ = new print_preview.OtherOptionsSettings(
        this.printTicketStore_);
    this.addChild(this.otherOptionsSettings_);

    /**
     * Area of the UI that holds the print preview.
     * @type {!print_preview.PreviewArea}
     * @private
     */
    this.previewArea_ = new print_preview.PreviewArea(
        this.destinationStore_, this.printTicketStore_, this.nativeLayer_);
    this.addChild(this.previewArea_);

    /**
     * Interface to the Google Cloud Print API. Null if Google Cloud Print
     * integration is disabled.
     * @type {cloudprint.CloudPrintInterface}
     * @private
     */
    this.cloudPrintInterface_ = null;

    /**
     * Whether in kiosk mode where print preview can print automatically without
     * user intervention. See http://crbug.com/31395. Print will start when
     * both the print ticket has been initialized, and an initial printer has
     * been selected.
     * @type {boolean}
     * @private
     */
    this.isInKioskAutoPrintMode_ = false;

    /**
     * State of the print preview UI.
     * @type {print_preview.PrintPreview.UiState_}
     * @private
     */
    this.uiState_ = PrintPreview.UiState_.INITIALIZING;

    /**
     * Whether document preview generation is in progress.
     * @type {boolean}
     * @private
     */
    this.isPreviewGenerationInProgress_ = true;
  };

  /**
   * States of the print preview.
   * @enum {string}
   * @private
   */
  PrintPreview.UiState_ = {
    INITIALIZING: 'initializing',
    READY: 'ready',
    OPENING_PDF_PREVIEW: 'opening-pdf-preview',
    OPENING_NATIVE_PRINT_DIALOG: 'opening-native-print-dialog',
    PRINTING: 'printing',
    FILE_SELECTION: 'file-selection',
    CLOSING: 'closing',
    ERROR: 'error'
  };

  PrintPreview.prototype = {
    __proto__: print_preview.Component.prototype,

    /** Sets up the page and print preview by getting the printer list. */
    initialize: function() {
      this.decorate($('print-preview'));
      i18nTemplate.process(document, templateData);
      if (!this.previewArea_.hasCompatiblePlugin) {
        this.setIsEnabled_(false);
      }
      this.nativeLayer_.startGetInitialSettings();
      this.destinationStore_.startLoadLocalDestinations();
    },

    /** @override */
    enterDocument: function() {
      // Native layer events.
      this.tracker.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET,
          this.onInitialSettingsSet_.bind(this));
      this.tracker.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.CLOUD_PRINT_ENABLE,
          this.onCloudPrintEnable_.bind(this));
      this.tracker.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PRINT_TO_CLOUD,
          this.onPrintToCloud_.bind(this));
      this.tracker.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.FILE_SELECTION_CANCEL,
          this.onFileSelectionCancel_.bind(this));
      this.tracker.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.FILE_SELECTION_COMPLETE,
          this.onFileSelectionComplete_.bind(this));
      this.tracker.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.SETTINGS_INVALID,
          this.onSettingsInvalid_.bind(this));
      this.tracker.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.DISABLE_SCALING,
          this.onDisableScaling_.bind(this));

      this.tracker.add(
          $('system-dialog-link'),
          'click',
          this.openSystemPrintDialog_.bind(this));
      this.tracker.add(
          $('cloud-print-dialog-link'),
          'click',
          this.onCloudPrintDialogLinkClick_.bind(this));
      this.tracker.add(
          $('open-pdf-in-preview-link'),
          'click',
          this.onOpenPdfInPreviewLinkClick_.bind(this));

      this.tracker.add(
          this.previewArea_,
          print_preview.PreviewArea.EventType.PREVIEW_GENERATION_IN_PROGRESS,
          this.onPreviewGenerationInProgress_.bind(this));
      this.tracker.add(
          this.previewArea_,
          print_preview.PreviewArea.EventType.PREVIEW_GENERATION_DONE,
          this.onPreviewGenerationDone_.bind(this));
      this.tracker.add(
          this.previewArea_,
          print_preview.PreviewArea.EventType.PREVIEW_GENERATION_FAIL,
          this.onPreviewGenerationFail_.bind(this));
      this.tracker.add(
          this.previewArea_,
          print_preview.PreviewArea.EventType.OPEN_SYSTEM_DIALOG_CLICK,
          this.openSystemPrintDialog_.bind(this));

      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.
              SELECTED_DESTINATION_CAPABILITIES_READY,
          this.printIfReady_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          this.onDestinationSelect_.bind(this));

      this.tracker.add(
          this.printHeader_,
          print_preview.PrintHeader.EventType.PRINT_BUTTON_CLICK,
          this.onPrintButtonClick_.bind(this));
      this.tracker.add(
          this.printHeader_,
          print_preview.PrintHeader.EventType.CANCEL_BUTTON_CLICK,
          this.onCancelButtonClick_.bind(this));

      this.tracker.add(window, 'keydown', this.onKeyDown_.bind(this));

      this.tracker.add(
          this.destinationSettings_,
          print_preview.DestinationSettings.EventType.CHANGE_BUTTON_ACTIVATE,
          this.onDestinationChangeButtonActivate_.bind(this));

      this.tracker.add(
          this.destinationSearch_,
          print_preview.DestinationSearch.EventType.MANAGE_CLOUD_DESTINATIONS,
          this.onManageCloudDestinationsActivated_.bind(this));
      this.tracker.add(
          this.destinationSearch_,
          print_preview.DestinationSearch.EventType.MANAGE_LOCAL_DESTINATIONS,
          this.onManageLocalDestinationsActivated_.bind(this));
      this.tracker.add(
          this.destinationSearch_,
          print_preview.DestinationSearch.EventType.SIGN_IN,
          this.onCloudPrintSignInActivated_.bind(this));
    },

    /** @override */
    decorateInternal: function() {
      this.printHeader_.decorate($('print-header'));
      this.destinationSearch_.decorate($('destination-search'));
      this.destinationSettings_.decorate($('destination-settings'));
      this.pageSettings_.decorate($('page-settings'));
      this.copiesSettings_.decorate($('copies-settings'));
      this.layoutSettings_.decorate($('layout-settings'));
      this.colorSettings_.decorate($('color-settings'));
      this.marginSettings_.decorate($('margin-settings'));
      this.otherOptionsSettings_.decorate($('other-options-settings'));
      this.previewArea_.decorate($('preview-area'));

      setIsVisible($('open-pdf-in-preview-link'), cr.isMac);
    },

    /**
     * Sets whether the controls in the print preview are enabled.
     * @param {boolean} isEnabled Whether the controls in the print preview are
     *     enabled.
     * @private
     */
    setIsEnabled_: function(isEnabled) {
      $('system-dialog-link').disabled = !isEnabled;
      $('cloud-print-dialog-link').disabled = !isEnabled;
      $('open-pdf-in-preview-link').disabled = !isEnabled;
      this.printHeader_.isEnabled = isEnabled;
      this.destinationSettings_.isEnabled = isEnabled;
      this.pageSettings_.isEnabled = isEnabled;
      this.copiesSettings_.isEnabled = isEnabled;
      this.layoutSettings_.isEnabled = isEnabled;
      this.colorSettings_.isEnabled = isEnabled;
      this.marginSettings_.isEnabled = isEnabled;
      this.otherOptionsSettings_.isEnabled = isEnabled;
    },

    /**
     * Prints the document or launches a pdf preview on the local system.
     * @param {boolean} isPdfPreview Whether to launch the pdf preview.
     * @private
     */
    printDocumentOrOpenPdfPreview_: function(isPdfPreview) {
      assert(this.uiState_ == PrintPreview.UiState_.READY,
             'Print document request received when not in ready state: ' +
                 this.uiState_);
      if (isPdfPreview) {
        this.uiState_ = PrintPreview.UiState_.OPENING_PDF_PREVIEW;
      } else if (this.destinationStore_.selectedDestination.id ==
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF) {
        this.uiState_ = PrintPreview.UiState_.FILE_SELECTION;
      } else {
        this.uiState_ = PrintPreview.UiState_.PRINTING;
      }
      this.setIsEnabled_(false);
      if (this.printIfReady_() &&
          ((this.destinationStore_.selectedDestination.isLocal &&
            this.destinationStore_.selectedDestination.id !=
                print_preview.Destination.GooglePromotedId.SAVE_AS_PDF) ||
           this.uiState_ == PrintPreview.UiState_.OPENING_PDF_PREVIEW)) {
        // Hide the dialog for now. The actual print command will be issued when
        // the preview generation is done.
        this.nativeLayer_.startHideDialog();
      }
    },

    /**
     * Attempts to print if needed and if ready.
     * @return {boolean} Whether a print request was issued.
     * @private
     */
    printIfReady_: function() {
      if ((this.uiState_ == PrintPreview.UiState_.PRINTING ||
              this.uiState_ == PrintPreview.UiState_.OPENING_PDF_PREVIEW ||
              this.uiState_ == PrintPreview.UiState_.FILE_SELECTION ||
              this.isInKioskAutoPrintMode_) &&
          !this.isPreviewGenerationInProgress_ &&
          this.destinationStore_.selectedDestination &&
          this.destinationStore_.selectedDestination.capabilities) {
        assert(this.printTicketStore_.isTicketValid(),
               'Trying to print with invalid ticket');
        this.nativeLayer_.startPrint(
            this.destinationStore_.selectedDestination,
            this.printTicketStore_,
            this.cloudPrintInterface_,
            this.uiState_ == PrintPreview.UiState_.OPENING_PDF_PREVIEW);
        return true;
      } else {
        return false;
      }
    },

    /**
     * Closes the print preview.
     * @private
     */
    close_: function() {
      this.exitDocument();
      this.uiState_ = PrintPreview.UiState_.CLOSING;
      this.nativeLayer_.startCloseDialog();
    },

    /**
     * Opens the native system print dialog after disabling all controls.
     * @private
     */
    openSystemPrintDialog_: function() {
      setIsVisible($('system-dialog-throbber'), true);
      this.setIsEnabled_(false);
      this.uiState_ = PrintPreview.UiState_.OPENING_NATIVE_PRINT_DIALOG;
      this.nativeLayer_.startShowSystemDialog();
    },

    /**
     * Called when the native layer has initial settings to set. Sets the
     * initial settings of the print preview and begins fetching print
     * destinations.
     * @param {cr.Event} event Contains the initial print preview settings
     *     persisted through the session.
     * @private
     */
    onInitialSettingsSet_: function(event) {
      assert(this.uiState_ == PrintPreview.UiState_.INITIALIZING,
             'Updating initial settings when not in initializing state: ' +
                 this.uiState_);
      this.uiState_ = PrintPreview.UiState_.READY;

      var settings = event.initialSettings;
      this.isInKioskAutoPrintMode_ = settings.isInKioskAutoPrintMode;
      document.title = settings.documentTitle;

      // The following components must be initialized in this order.
      this.appState_.init(settings.serializedAppStateStr);
      this.printTicketStore_.init(
          settings.isDocumentModifiable,
          settings.documentTitle,
          settings.thousandsDelimeter,
          settings.decimalDelimeter,
          settings.unitType);
      this.destinationStore_.init(settings.systemDefaultDestinationId);
    },

    /**
     * Calls when the native layer enables Google Cloud Print integration.
     * Fetches the user's cloud printers.
     * @param {cr.Event} event Contains the base URL of the Google Cloud Print
     *     service.
     * @private
     */
    onCloudPrintEnable_: function(event) {
      this.cloudPrintInterface_ =
          new cloudprint.CloudPrintInterface(event.baseCloudPrintUrl);
      this.tracker.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.SUBMIT_DONE,
          this.onCloudPrintSubmitDone_.bind(this));
      this.tracker.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.SEARCH_FAILED,
          this.onCloudPrintError_.bind(this));
      this.tracker.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.SUBMIT_FAILED,
          this.onCloudPrintError_.bind(this));
      this.tracker.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.PRINTER_FAILED,
          this.onCloudPrintError_.bind(this));
      this.tracker.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.
              UPDATE_PRINTER_TOS_ACCEPTANCE_FAILED,
          this.onCloudPrintError_.bind(this));

      this.userInfo_.setCloudPrintInterface(this.cloudPrintInterface_);
      this.destinationStore_.setCloudPrintInterface(this.cloudPrintInterface_);
      this.destinationStore_.startLoadRecentCloudDestinations();
      if (this.destinationSearch_.getIsVisible()) {
        this.destinationStore_.startLoadAllCloudDestinations();
      }
    },

    /**
     * Called from the native layer when ready to print to Google Cloud Print.
     * @param {cr.Event} event Contains the body to send in the HTTP request.
     * @private
     */
    onPrintToCloud_: function(event) {
      assert(this.uiState_ == PrintPreview.UiState_.PRINTING,
             'Document ready to be sent to the cloud when not in printing ' +
                 'state: ' + this.uiState_);
      assert(this.cloudPrintInterface_ != null,
             'Google Cloud Print is not enabled');
      this.cloudPrintInterface_.submit(event.data);
    },

    /**
     * Called from the native layer when the user cancels the save-to-pdf file
     * selection dialog.
     * @private
     */
    onFileSelectionCancel_: function() {
      assert(this.uiState_ == PrintPreview.UiState_.FILE_SELECTION,
             'File selection cancelled when not in file-selection state: ' +
                 this.uiState_);
      this.setIsEnabled_(true);
      this.uiState_ = PrintPreview.UiState_.READY;
    },

    /**
     * Called from the native layer when save-to-pdf file selection is complete.
     * @private
     */
    onFileSelectionComplete_: function() {
      assert(this.uiState_ == PrintPreview.UiState_.FILE_SELECTION,
             'File selection completed when not in file-selection state: ' +
                 this.uiState_);
      this.previewArea_.showCustomMessage(
          localStrings.getString('printingToPDFInProgress'));
      this.uiState_ = PrintPreview.UiState_.PRINTING;
    },

    /**
     * Called after successfully submitting a job to Google Cloud Print.
     * @param {!cr.Event} event Contains the ID of the submitted print job.
     * @private
     */
    onCloudPrintSubmitDone_: function(event) {
      assert(this.uiState_ == PrintPreview.UiState_.PRINTING,
             'Submited job to Google Cloud Print but not in printing state ' +
                 this.uiState_);
      if (this.destinationStore_.selectedDestination.id ==
              print_preview.Destination.GooglePromotedId.FEDEX) {
        window.open(
            'https://www.google.com/cloudprint/fedexcode.html?jobid=' +
            event.jobId);
      }
      this.close_();
    },

    /**
     * Called when there was an error communicating with Google Cloud print.
     * Displays an error message in the print header.
     * @param {!cr.Event} event Contains the error message.
     * @private
     */
    onCloudPrintError_: function(event) {
      if (event.status == 403) {
        this.destinationSearch_.showCloudPrintPromo();
      } else if (event.status == 0) {
        return; // Ignore, the system does not have internet connectivity.
      } else {
        this.printHeader_.setErrorMessage(event.message);
      }
      if (event.status == 200) {
        console.error('Google Cloud Print Error: (' + event.errorCode + ') ' +
                      event.message);
      } else {
        console.error('Google Cloud Print Error: HTTP status ' + event.status);
      }
    },

    /**
     * Called when the preview area's preview generation is in progress.
     * @private
     */
    onPreviewGenerationInProgress_: function() {
      this.isPreviewGenerationInProgress_ = true;
    },

    /**
     * Called when the preview area's preview generation is complete.
     * @private
     */
    onPreviewGenerationDone_: function() {
      this.isPreviewGenerationInProgress_ = false;
      this.printIfReady_();
    },

    /**
     * Called when the preview area's preview failed to load.
     * @private
     */
    onPreviewGenerationFail_: function() {
      this.isPreviewGenerationInProgress_ = false;
      if (this.uiState_ == PrintPreview.UiState_.PRINTING) {
        this.nativeLayer_.startCancelPendingPrint();
      }
    },

    /**
     * Called when the 'Open pdf in preview' link is clicked. Launches the pdf
     * preview app.
     * @private
     */
    onOpenPdfInPreviewLinkClick_: function() {
      assert(this.uiState_ == PrintPreview.UiState_.READY,
             'Trying to open pdf in preview when not in ready state: ' +
                 this.uiState_);
      setIsVisible($('open-preview-app-throbber'), true);
      this.previewArea_.showCustomMessage(
          localStrings.getString('openingPDFInPreview'));
      this.printDocumentOrOpenPdfPreview_(true /*isPdfPreview*/);
    },

    /**
     * Called when the print header's print button is clicked. Prints the
     * document.
     * @private
     */
    onPrintButtonClick_: function() {
      assert(this.uiState_ == PrintPreview.UiState_.READY,
             'Trying to print when not in ready state: ' + this.uiState_);
      this.printDocumentOrOpenPdfPreview_(false /*isPdfPreview*/);
    },

    /**
     * Called when the print header's cancel button is clicked. Closes the
     * print dialog.
     * @private
     */
    onCancelButtonClick_: function() {
      this.close_();
    },

    /**
     * Consume escape key presses and ctrl + shift + p. Delegate everything else
     * to the preview area.
     * @param {KeyboardEvent} e The keyboard event.
     * @private
     */
    onKeyDown_: function(e) {
      // Escape key closes the dialog.
      // <if expr="not pp_ifdef('toolkit_views')">
      // On the toolkit_views environment, ESC key is handled by C++-side
      // instead of JS-side.
      if (e.keyCode == 27 && !e.shiftKey && !e.ctrlKey && !e.altKey &&
          !e.metaKey) {
        if (this.destinationSearch_.getIsVisible()) {
          this.destinationSearch_.setIsVisible(false);
          this.metrics_.increment(
              print_preview.Metrics.Bucket.DESTINATION_SELECTION_CANCELED);
        } else {
          this.close_();
        }
        e.preventDefault();
        return;
      }
      // </if>

      // Ctrl + Shift + p / Mac equivalent.
      if (e.keyCode == 80) {
        if ((cr.isMac && e.metaKey && e.altKey && !e.shiftKey && !e.ctrlKey) ||
            (!cr.isMac && e.shiftKey && e.ctrlKey && !e.altKey && !e.metaKey)) {
          this.openSystemPrintDialog_();
          e.preventDefault();
          return;
        }
      }

      if (e.keyCode == 13 /*enter*/ &&
          !this.destinationSearch_.getIsVisible() &&
          this.printTicketStore_.isTicketValid()) {
        assert(this.uiState_ == PrintPreview.UiState_.READY,
          'Trying to print when not in ready state: ' + this.uiState_);
        this.printDocumentOrOpenPdfPreview_(false /*isPdfPreview*/);
        e.preventDefault();
        return;
      }

      // Pass certain directional keyboard events to the PDF viewer.
      this.previewArea_.handleDirectionalKeyEvent(e);
    },

    /**
     * Called when native layer receives invalid settings for a print request.
     * @private
     */
    onSettingsInvalid_: function() {
      this.uiState_ = PrintPreview.UiState_.ERROR;
      console.error('Invalid settings error reported from native layer');
      this.previewArea_.showCustomMessage(
          localStrings.getString('invalidPrinterSettings'));
    },

    /**
     * Called when the destination settings' change button is activated.
     * Displays the destination search component.
     * @private
     */
    onDestinationChangeButtonActivate_: function() {
      this.destinationSearch_.setIsVisible(true);
      this.destinationStore_.startLoadAllCloudDestinations();
      this.metrics_.increment(
          print_preview.Metrics.Bucket.DESTINATION_SEARCH_SHOWN);
    },

    /**
     * Called when the destination search dispatches manage cloud destinations
     * event. Calls corresponding native layer method.
     * @private
     */
    onManageCloudDestinationsActivated_: function() {
      this.nativeLayer_.startManageCloudDestinations();
    },

    /**
     * Called when the destination search dispatches manage local destinations
     * event. Calls corresponding native layer method.
     * @private
     */
    onManageLocalDestinationsActivated_: function() {
      this.nativeLayer_.startManageLocalDestinations();
    },

    /**
     * Called when the user wants to sign in to Google Cloud Print. Calls the
     * corresponding native layer event.
     * @private
     */
    onCloudPrintSignInActivated_: function() {
      this.nativeLayer_.startCloudPrintSignIn();
    },

    /**
     * Called when the native layer dispatches a DISABLE_SCALING event. Updates
     * the print ticket.
     * @private
     */
    onDisableScaling_: function() {
      this.printTicketStore_.updateFitToPage(false);
    },

    /**
     * Called when the open-cloud-print-dialog link is clicked. Opens the Google
     * Cloud Print web dialog.
     * @private
     */
    onCloudPrintDialogLinkClick_: function() {
      assert(this.uiState_ == PrintPreview.UiState_.READY,
             'Opening Google Cloud Print dialog when not in ready state: ' +
                 this.uiState_);
      setIsVisible($('cloud-print-dialog-throbber'), true);
      this.setIsEnabled_(false);
      this.uiState_ = PrintPreview.UiState_.OPENING_NATIVE_PRINT_DIALOG;
      this.nativeLayer_.startShowCloudPrintDialog();
    },

    /**
     * Called when a print destination is selected. Shows/hides the "Print with
     * Cloud Print" link in the navbar.
     * @private
     */
    onDestinationSelect_: function() {
      var selectedDest = this.destinationStore_.selectedDestination;
      setIsVisible($('cloud-print-dialog-link'),
                   !cr.isChromeOS && !selectedDest.isLocal);
    }
  };

  // Export
  return {
    PrintPreview: PrintPreview
  };
});

// Pull in all other scripts in a single shot.
<include src="data/page_number_set.js"/>
<include src="data/destination.js"/>
<include src="data/local_parsers.js"/>
<include src="data/cloud_parsers.js"/>
<include src="data/chromium_capabilities.js"/>
<include src="data/cloud_capabilities.js"/>
<include src="data/destination_store.js"/>
<include src="data/margins.js"/>
<include src="data/document_info.js"/>
<include src="data/printable_area.js"/>
<include src="data/measurement_system.js"/>
<include src="data/print_ticket_store.js"/>
<include src="data/coordinate2d.js"/>
<include src="data/size.js"/>
<include src="data/capabilities_holder.js"/>
<include src="data/user_info.js"/>
<include src="data/app_state.js"/>

<include src="data/ticket_items/ticket_item.js"/>

<include src="data/ticket_items/custom_margins.js"/>
<include src="data/ticket_items/collate.js"/>
<include src="data/ticket_items/color.js"/>
<include src="data/ticket_items/copies.js"/>
<include src="data/ticket_items/duplex.js"/>
<include src="data/ticket_items/header_footer.js"/>
<include src="data/ticket_items/landscape.js"/>
<include src="data/ticket_items/margins_type.js"/>
<include src="data/ticket_items/page_range.js"/>
<include src="data/ticket_items/fit_to_page.js"/>

<include src="native_layer.js"/>
<include src="print_preview_animations.js"/>
<include src="cloud_print_interface.js"/>
<include src="print_preview_utils.js"/>
<include src="print_header.js"/>
<include src="metrics.js"/>

<include src="settings/page_settings.js"/>
<include src="settings/copies_settings.js"/>
<include src="settings/layout_settings.js"/>
<include src="settings/color_settings.js"/>
<include src="settings/margin_settings.js"/>
<include src="settings/destination_settings.js"/>
<include src="settings/other_options_settings.js"/>

<include src="previewarea/margin_control.js"/>
<include src="previewarea/margin_control_container.js"/>
<include src="previewarea/preview_area.js"/>
<include src="preview_generator.js"/>

<include src="search/destination_list.js"/>
<include src="search/cloud_destination_list.js"/>
<include src="search/recent_destination_list.js"/>
<include src="search/destination_list_item.js"/>
<include src="search/destination_search.js"/>
<include src="search/search_box.js"/>
<include src="search/fedex_tos.js"/>

window.addEventListener('DOMContentLoaded', function() {
  printPreview = new print_preview.PrintPreview();
  printPreview.initialize();
});

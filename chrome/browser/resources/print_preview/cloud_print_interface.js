// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cloudprint', function() {
  'use strict';

  /**
   * API to the Google Cloud Print service.
   * @param {string} baseUrl Base part of the Google Cloud Print service URL
   *     with no trailing slash. For example,
   *     'https://www.google.com/cloudprint'.
   * @param {!print_preview.NativeLayer} nativeLayer Native layer used to get
   *     Auth2 tokens.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function CloudPrintInterface(baseUrl, nativeLayer) {
    /**
     * The base URL of the Google Cloud Print API.
     * @type {string}
     * @private
     */
    this.baseUrl_ = baseUrl;

    /**
     * Used to get Auth2 tokens.
     * @type {!print_preview.NativeLayer}
     * @private
     */
    this.nativeLayer_ = nativeLayer;

    /**
     * Last received XSRF token. Sent as a parameter in every request.
     * @type {string}
     * @private
     */
    this.xsrfToken_ = '';

    /**
     * Pending requests delayed until we get access token.
     * @type {!Array.<!CloudPrintRequest>}
     * @private
     */
    this.requestQueue_ = [];

    /**
     * Number of outstanding cloud destination search requests.
     * @type {number}
     * @private
     */
    this.outstandingCloudSearchRequestCount_ = 0;

    /**
     * Event tracker used to keep track of native layer events.
     * @type {!EventTracker}
     * @private
     */
    this.tracker_ = new EventTracker();

    this.addEventListeners_();
  };

  /**
   * Event types dispatched by the interface.
   * @enum {string}
   */
  CloudPrintInterface.EventType = {
    PRINTER_DONE: 'cloudprint.CloudPrintInterface.PRINTER_DONE',
    PRINTER_FAILED: 'cloudprint.CloudPrintInterface.PRINTER_FAILED',
    SEARCH_DONE: 'cloudprint.CloudPrintInterface.SEARCH_DONE',
    SEARCH_FAILED: 'cloudprint.CloudPrintInterface.SEARCH_FAILED',
    SUBMIT_DONE: 'cloudprint.CloudPrintInterface.SUBMIT_DONE',
    SUBMIT_FAILED: 'cloudprint.CloudPrintInterface.SUBMIT_FAILED',
    UPDATE_PRINTER_TOS_ACCEPTANCE_FAILED:
        'cloudprint.CloudPrintInterface.UPDATE_PRINTER_TOS_ACCEPTANCE_FAILED'
  };

  /**
   * Content type header value for a URL encoded HTTP request.
   * @type {string}
   * @const
   * @private
   */
  CloudPrintInterface.URL_ENCODED_CONTENT_TYPE_ =
      'application/x-www-form-urlencoded';

  /**
   * Multi-part POST request boundary used in communication with Google
   * Cloud Print.
   * @type {string}
   * @const
   * @private
   */
  CloudPrintInterface.MULTIPART_BOUNDARY_ =
      '----CloudPrintFormBoundaryjc9wuprokl8i';

  /**
   * Content type header value for a multipart HTTP request.
   * @type {string}
   * @const
   * @private
   */
  CloudPrintInterface.MULTIPART_CONTENT_TYPE_ =
      'multipart/form-data; boundary=' +
      CloudPrintInterface.MULTIPART_BOUNDARY_;

  /**
   * Regex that extracts Chrome's version from the user-agent string.
   * @type {!RegExp}
   * @const
   * @private
   */
  CloudPrintInterface.VERSION_REGEXP_ = /.*Chrome\/([\d\.]+)/i;

  /**
   * Enumeration of JSON response fields from Google Cloud Print API.
   * @enum {string}
   * @private
   */
  CloudPrintInterface.JsonFields_ = {
    PRINTER: 'printer'
  };

  /**
   * Could Print origins used to search printers.
   * @type {!Array.<!print_preview.Destination.Origin>}
   * @const
   * @private
   */
  CloudPrintInterface.CLOUD_ORIGINS_ = [
      print_preview.Destination.Origin.COOKIES,
      print_preview.Destination.Origin.DEVICE
      // TODO(vitalybuka): Enable when implemented.
      // ready print_preview.Destination.Origin.PROFILE
  ];

  CloudPrintInterface.prototype = {
    __proto__: cr.EventTarget.prototype,

    /** @return {string} Base URL of the Google Cloud Print service. */
    get baseUrl() {
      return this.baseUrl_;
    },

    /**
     * @return {boolean} Whether a search for cloud destinations is in progress.
     */
    get isCloudDestinationSearchInProgress() {
      return this.outstandingCloudSearchRequestCount_ > 0;
    },

    /**
     * Sends a Google Cloud Print search API request.
     * @param {boolean} isRecent Whether to search for only recently used
     *     printers.
     */
    search: function(isRecent) {
      var params = [
        new HttpParam('connection_status', 'ALL'),
        new HttpParam('client', 'chrome'),
        new HttpParam('use_cdd', 'true')
      ];
      if (isRecent) {
        params.push(new HttpParam('q', '^recent'));
      }
      CloudPrintInterface.CLOUD_ORIGINS_.forEach(function(origin) {
        ++this.outstandingCloudSearchRequestCount_;
        var cpRequest =
            this.buildRequest_('GET', 'search', params, origin,
                               this.onSearchDone_.bind(this, isRecent));
        this.sendOrQueueRequest_(cpRequest);
      }, this);
    },

    /**
     * Sends a Google Cloud Print submit API request.
     * @param {!print_preview.Destination} destination Cloud destination to
     *     print to.
     * @param {!print_preview.PrintTicketStore} printTicketStore Contains the
     *     print ticket to print.
     * @param {!print_preview.DocumentInfo} documentInfo Document data model.
     * @param {string} data Base64 encoded data of the document.
     */
    submit: function(destination, printTicketStore, documentInfo, data) {
      var result =
          CloudPrintInterface.VERSION_REGEXP_.exec(navigator.userAgent);
      var chromeVersion = 'unknown';
      if (result && result.length == 2) {
        chromeVersion = result[1];
      }
      var params = [
        new HttpParam('printerid', destination.id),
        new HttpParam('contentType', 'dataUrl'),
        new HttpParam('title', documentInfo.title),
        new HttpParam('ticket',
                      this.createPrintTicket_(destination, printTicketStore)),
        new HttpParam('content', 'data:application/pdf;base64,' + data),
        new HttpParam('tag',
                      '__google__chrome_version=' + chromeVersion),
        new HttpParam('tag', '__google__os=' + navigator.platform)
      ];
      var cpRequest = this.buildRequest_('POST', 'submit', params,
                                         destination.origin,
                                         this.onSubmitDone_.bind(this));
      this.sendOrQueueRequest_(cpRequest);
    },

    /**
     * Sends a Google Cloud Print printer API request.
     * @param {string} printerId ID of the printer to lookup.
     * @param {!print_preview.Destination.Origin} origin Origin of the printer.
     */
    printer: function(printerId, origin) {
      var params = [
        new HttpParam('printerid', printerId),
        new HttpParam('use_cdd', 'true')
      ];
      var cpRequest =
          this.buildRequest_('GET', 'printer', params, origin,
                             this.onPrinterDone_.bind(this, printerId));
      this.sendOrQueueRequest_(cpRequest);
    },

    /**
     * Sends a Google Cloud Print update API request to accept (or reject) the
     * terms-of-service of the given printer.
     * @param {string} printerId ID of the printer to accept the
     *     terms-of-service for.
     * @param {!print_preview.Destination.Origin} origin Origin of the printer.
     * @param {boolean} isAccepted Whether the user accepted the
     *     terms-of-service.
     */
    updatePrinterTosAcceptance: function(printerId, origin, isAccepted) {
      var params = [
        new HttpParam('printerid', printerId),
        new HttpParam('is_tos_accepted', isAccepted)
      ];
      var cpRequest =
          this.buildRequest_('POST', 'update', params, origin,
                             this.onUpdatePrinterTosAcceptanceDone_.bind(this));
      this.sendOrQueueRequest_(cpRequest);
    },

    /**
     * Adds event listeners to the relevant native layer events.
     * @private
     */
    addEventListeners_: function() {
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.ACCESS_TOKEN_READY,
          this.onAccessTokenReady_.bind(this));
    },

    /**
     * Creates an object that represents a Google Cloud Print print ticket.
     * @param {!print_preview.Destination} destination Destination to print to.
     * @param {!print_preview.PrintTicketStore} printTicketStore Used to create
     *     the state of the print ticket.
     * @return {!Object} Google Cloud Print print ticket.
     * @private
     */
    createPrintTicket_: function(destination, printTicketStore) {
      assert(!destination.isLocal,
             'Trying to create a Google Cloud Print print ticket for a local ' +
                 'destination');
      assert(destination.capabilities,
             'Trying to create a Google Cloud Print print ticket for a ' +
                 'destination with no print capabilities');
      var pts = printTicketStore; // For brevity.
      var cjt = {
        version: '1.0',
        print: {}
      };
      if (pts.collate.isCapabilityAvailable() && pts.collate.isUserEdited()) {
        cjt.print.collate = {collate: pts.collate.getValue() == 'true'};
      }
      if (pts.color.isCapabilityAvailable() && pts.color.isUserEdited()) {
        var colorType = pts.color.getValue() ?
            'STANDARD_COLOR' : 'STANDARD_MONOCHROME';
        // Find option with this colorType to read its vendor_id.
        var selectedOptions = destination.capabilities.printer.color.option.
            filter(function(option) {
              return option.type == colorType;
            });
        if (selectedOptions.length == 0) {
          console.error('Could not find correct color option');
        } else {
          cjt.print.color = {type: colorType};
          if (selectedOptions[0].hasOwnProperty('vendor_id')) {
            cjt.print.color.vendor_id = selectedOptions[0].vendor_id;
          }
        }
      }
      if (pts.copies.isCapabilityAvailable() && pts.copies.isUserEdited()) {
        cjt.print.copies = {copies: pts.copies.getValueAsNumber()};
      }
      if (pts.duplex.isCapabilityAvailable() && pts.duplex.isUserEdited()) {
        cjt.print.duplex =
            {type: pts.duplex.getValue() ? 'LONG_EDGE' : 'NO_DUPLEX'};
      }
      if (pts.hasOrientationCapability()) {
        cjt.print.page_orientation =
          {type: pts.isLandscapeEnabled() ? 'LANDSCAPE' : 'PORTRAIT'};
      }
      return JSON.stringify(cjt);
    },

    /**
     * Builds request to the Google Cloud Print API.
     * @param {string} method HTTP method of the request.
     * @param {string} action Google Cloud Print action to perform.
     * @param {Array.<!HttpParam>} params HTTP parameters to include in the
     *     request.
     * @param {!print_preview.Destination.Origin} origin Origin for destination.
     * @param {function(number, Object, !print_preview.Destination.Origin)}
     *     callback Callback to invoke when request completes.
     * @return {!CloudPrintRequest} Partially prepared request.
     * @private
     */
    buildRequest_: function(method, action, params, origin, callback) {
      var url = this.baseUrl_ + '/' + action + '?xsrf=';
      if (origin == print_preview.Destination.Origin.COOKIES) {
        if (!this.xsrfToken_) {
          // TODO(rltoscano): Should throw an error if not a read-only action or
          // issue an xsrf token request.
        } else {
          url = url + this.xsrfToken_;
        }
      }
      var body = null;
      if (params) {
        if (method == 'GET') {
          url = params.reduce(function(partialUrl, param) {
            return partialUrl + '&' + param.name + '=' +
                encodeURIComponent(param.value);
          }, url);
        } else if (method == 'POST') {
          body = params.reduce(function(partialBody, param) {
            return partialBody + 'Content-Disposition: form-data; name=\"' +
                param.name + '\"\r\n\r\n' + param.value + '\r\n--' +
                CloudPrintInterface.MULTIPART_BOUNDARY_ + '\r\n';
          }, '--' + CloudPrintInterface.MULTIPART_BOUNDARY_ + '\r\n');
        }
      }

      var headers = {};
      headers['X-CloudPrint-Proxy'] = 'ChromePrintPreview';
      if (method == 'GET') {
        headers['Content-Type'] = CloudPrintInterface.URL_ENCODED_CONTENT_TYPE_;
      } else if (method == 'POST') {
        headers['Content-Type'] = CloudPrintInterface.MULTIPART_CONTENT_TYPE_;
      }

      var xhr = new XMLHttpRequest();
      xhr.open(method, url, true);
      xhr.withCredentials =
          (origin == print_preview.Destination.Origin.COOKIES);
      for (var header in headers) {
        xhr.setRequestHeader(header, headers[header]);
      }

      return new CloudPrintRequest(xhr, body, origin, callback);
    },

    /**
     * Sends a request to the Google Cloud Print API or queues if it needs to
     *     wait OAuth2 access token.
     * @param {!CloudPrintRequest} request Request to send or queue.
     * @private
     */
    sendOrQueueRequest_: function(request) {
      if (request.origin == print_preview.Destination.Origin.COOKIES) {
        return this.sendRequest_(request);
      } else {
        this.requestQueue_.push(request);
        this.nativeLayer_.startGetAccessToken(request.origin);
      }
    },

    /**
     * Sends a request to the Google Cloud Print API.
     * @param {!CloudPrintRequest} request Request to send.
     * @private
     */
    sendRequest_: function(request) {
      request.xhr.onreadystatechange =
          this.onReadyStateChange_.bind(this, request);
      request.xhr.send(request.body);
    },

    /**
     * Creates a Google Cloud Print interface error that is ready to dispatch.
     * @param {!CloudPrintInterface.EventType} type Type of the error.
     * @param {!CloudPrintRequest} request Request that has been completed.
     * @return {!cr.Event} Google Cloud Print interface error event.
     * @private
     */
    createErrorEvent_: function(type, request) {
      var errorEvent = new cr.Event(type);
      errorEvent.status = request.xhr.status;
      if (request.xhr.status == 200) {
        errorEvent.errorCode = request.result['errorCode'];
        errorEvent.message = request.result['message'];
      } else {
        errorEvent.errorCode = 0;
        errorEvent.message = '';
      }
      errorEvent.origin = request.origin;
      return errorEvent;
    },

    /**
     * Called when a native layer receives access token.
     * @param {cr.Event} evt Contains the authetication type and access token.
     * @private
     */
    onAccessTokenReady_: function(event) {
      // TODO(vitalybuka): remove when other Origins implemented.
      assert(event.authType == print_preview.Destination.Origin.DEVICE);
      this.requestQueue_ = this.requestQueue_.filter(function(request) {
        assert(request.origin == print_preview.Destination.Origin.DEVICE);
        if (request.origin != event.authType) {
          return true;
        }
        if (event.accessToken) {
          request.xhr.setRequestHeader('Authorization',
                                       'Bearer ' + event.accessToken);
          this.sendRequest_(request);
        } else {  // No valid token.
          // Without abort status does not exists.
          request.xhr.abort();
          request.callback(request);
        }
        return false;
      }, this);
    },

    /**
     * Called when the ready-state of a XML http request changes.
     * Calls the successCallback with the result or dispatches an ERROR event.
     * @param {!CloudPrintRequest} request Request that was changed.
     * @private
     */
    onReadyStateChange_: function(request) {
      if (request.xhr.readyState == 4) {
        if (request.xhr.status == 200) {
          request.result = JSON.parse(request.xhr.responseText);
          if (request.origin == print_preview.Destination.Origin.COOKIES &&
              request.result['success']) {
            this.xsrfToken_ = request.result['xsrf_token'];
          }
        }
        request.status = request.xhr.status;
        request.callback(request);
      }
    },

    /**
     * Called when the search request completes.
     * @param {boolean} isRecent Whether the search request was for recent
     *     destinations.
     * @param {!CloudPrintRequest} request Request that has been completed.
     * @private
     */
    onSearchDone_: function(isRecent, request) {
      --this.outstandingCloudSearchRequestCount_;
      if (request.xhr.status == 200 && request.result['success']) {
        var printerListJson = request.result['printers'] || [];
        var printerList = [];
        printerListJson.forEach(function(printerJson) {
          try {
            printerList.push(
                cloudprint.CloudDestinationParser.parse(printerJson,
                                                        request.origin));
          } catch (err) {
            console.error('Unable to parse cloud print destination: ' + err);
          }
        });
        var searchDoneEvent =
            new cr.Event(CloudPrintInterface.EventType.SEARCH_DONE);
        searchDoneEvent.printers = printerList;
        searchDoneEvent.origin = request.origin;
        searchDoneEvent.isRecent = isRecent;
        searchDoneEvent.email = request.result['request']['user'];
        this.dispatchEvent(searchDoneEvent);
      } else {
        var errorEvent = this.createErrorEvent_(
            CloudPrintInterface.EventType.SEARCH_FAILED, request);
        this.dispatchEvent(errorEvent);
      }
    },

    /**
     * Called when the submit request completes.
     * @param {!CloudPrintRequest} request Request that has been completed.
     * @private
     */
    onSubmitDone_: function(request) {
      if (request.xhr.status == 200 && request.result['success']) {
        var submitDoneEvent = new cr.Event(
            CloudPrintInterface.EventType.SUBMIT_DONE);
        submitDoneEvent.jobId = request.result['job']['id'];
        this.dispatchEvent(submitDoneEvent);
      } else {
        var errorEvent = this.createErrorEvent_(
            CloudPrintInterface.EventType.SUBMIT_FAILED, request);
        this.dispatchEvent(errorEvent);
      }
    },

    /**
     * Called when the printer request completes.
     * @param {string} destinationId ID of the destination that was looked up.
     * @param {!CloudPrintRequest} request Request that has been completed.
     * @private
     */
    onPrinterDone_: function(destinationId, request) {
      if (request.xhr.status == 200 && request.result['success']) {
        var printerJson = request.result['printers'][0];
        var printer;
        try {
          printer = cloudprint.CloudDestinationParser.parse(printerJson,
                                                            request.origin);
        } catch (err) {
          console.error('Failed to parse cloud print destination: ' +
              JSON.stringify(printerJson));
          return;
        }
        var printerDoneEvent =
            new cr.Event(CloudPrintInterface.EventType.PRINTER_DONE);
        printerDoneEvent.printer = printer;
        this.dispatchEvent(printerDoneEvent);
      } else {
        var errorEvent = this.createErrorEvent_(
            CloudPrintInterface.EventType.PRINTER_FAILED, request);
        errorEvent.destinationId = destinationId;
        errorEvent.destinationOrigin = request.origin;
        this.dispatchEvent(errorEvent, request.origin);
      }
    },

    /**
     * Called when the update printer TOS acceptance request completes.
     * @param {!CloudPrintRequest} request Request that has been completed.
     * @private
     */
    onUpdatePrinterTosAcceptanceDone_: function(request) {
      if (request.xhr.status == 200 && request.result['success']) {
        // Do nothing.
      } else {
        var errorEvent = this.createErrorEvent_(
            CloudPrintInterface.EventType.SUBMIT_FAILED, request);
        this.dispatchEvent(errorEvent);
      }
    }
  };

  /**
   * Data structure that holds data for Cloud Print requests.
   * @param {!XMLHttpRequest} xhr Partially prepared http request.
   * @param {string} body Data to send with POST requests.
   * @param {!print_preview.Destination.Origin} origin Origin for destination.
   * @param {function(!CloudPrintRequest)} callback Callback to invoke when
   *     request completes.
   * @constructor
   */
  function CloudPrintRequest(xhr, body, origin, callback) {
    /**
     * Partially prepared http request.
     * @type {!XMLHttpRequest}
     */
    this.xhr = xhr;

    /**
     * Data to send with POST requests.
     * @type {string}
     */
    this.body = body;

    /**
     * Origin for destination.
     * @type {!print_preview.Destination.Origin}
     */
    this.origin = origin;

    /**
     * Callback to invoke when request completes.
     * @type {function(!CloudPrintRequest)}
     */
    this.callback = callback;

    /**
     * Result for requests.
     * @type {Object} JSON response.
     */
    this.result = null;
  };

  /**
   * Data structure that represents an HTTP parameter.
   * @param {string} name Name of the parameter.
   * @param {string} value Value of the parameter.
   * @constructor
   */
  function HttpParam(name, value) {
    /**
     * Name of the parameter.
     * @type {string}
     */
    this.name = name;

    /**
     * Name of the value.
     * @type {string}
     */
    this.value = value;
  };

  // Export
  return {
    CloudPrintInterface: CloudPrintInterface
  };
});

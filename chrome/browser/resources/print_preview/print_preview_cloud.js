// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cloudprint', function() {

  // The URL to use to access the cloud print servers.
  // Set by a call to setBaseURL.
  var cloudPrintBaseURL = '';

  // Headers to set for most cloud print API calls.
  var xCloudPrintURLHeader = {'Content-Type':
                              'application/x-www-form-urlencoded',
                              'X-CloudPrint-Proxy': 'ChromePrintPreview'};

  // Headers to set when sending multipart data to cloud print APIs.
  // Currently only used when submitting a job.
  var xCloudPrintFormHeader = {'Content-Type':
        'multipart/form-data; boundary=----CloudPrintFormBoundaryjc9wuprokl8i',
        'X-CloudPrint-Proxy': 'ChromePrintPreview'};

  // The last received XSRF token.  This should be sent with each request
  // to prevent XSRF.
  var lastXSRFToken = '';

  /**
   * Sets the base URL to be used for communicating with cloud print
   * servers.
   * @param {string} cloudPrintURL The URL to use.
   */
  function setBaseURL(cloudPrintURL) {
    cloudPrintBaseURL = cloudPrintURL;
  }

  /**
   * Gets the base URL to be used for communicating with cloud print
   * servers.
   * @return {string} The URL.
   */
  function getBaseURL() {
    return cloudPrintBaseURL;
  }

  /**
   * Extracts the XSRF token from each response to be used in the next
   * request.
   * @param {XMLHttpRequest} xhr The object used to make the request.
   * @return {string} The extracted XSRF token.
   */
  function extractXSRFtoken(xhr) {
    if (xhr.status == 200) {
      var result = JSON.parse(xhr.responseText);
      return result['xsrf_token'];
    } else {
      return null;
    }
  }

  /**
   * Makes a request to cloud print servers.
   * @param {string} method The HTTP method to be used.
   * @param {string} action The cloud print API to call.
   * @param {Array} headers Headers to send with the request.
   * @param {string} body Body to be sent with POST requests.
   * @param {function} callback Function to be called to process response.
   * @param {boolean} async True if we want the request made asyncronously.
   */
  function sendCloudPrintRequest(method,
                                 action,
                                 headers,
                                 params,
                                 body,
                                 callback) {
    var xhr = new XMLHttpRequest();
    if (callback != null) {
      xhr.onreadystatechange = function() {
        if (xhr.readyState == 4) {
          var updatedXSRFToken = extractXSRFtoken(xhr);
          if (updatedXSRFToken != null) {
            lastXSRFToken = updatedXSRFToken;
          }
          callback.call(this, xhr);
        }
      };
    }
    var url = cloudPrintBaseURL + '/' + action;
    if (params == null) {
      params = new Array;
    }
    if (lastXSRFToken.length != 0) {
      params.push('xsrf=' + lastXSRFToken);
    }
    if (params.length != 0) {
      url = url + '?';
      for (param in params) {
        url = url + params[param] + '&';
      }
    }
    xhr.open(method, url, true);
    xhr.withCredentials = true;
    if (headers) {
      for (var header in headers) {
        if (headers.hasOwnProperty(header)) {
          xhr.setRequestHeader(header, headers[header]);
        }
      }
    }
    xhr.send(body);
    return xhr;
  }

  /**
   * Parse the response from the fetchPrinters call.
   * @param {function} callback Function to be called to process response.
   * @param {XMLHttpRequest} xhr The object used to make the request.
   */
  function fetchPrintersResponse(callback, xhr) {
    if (xhr.status == 200) {
      var searchResult = JSON.parse(xhr.responseText);
      if (searchResult['success']) {
        var printerList = searchResult['printers'];
        addCloudPrinters(printerList, callback);
        return;
      }
    }
    addCloudPrinters(null, callback);
  }

  /**
   * Retrieve the list of printers available via cloud print.
   * @param {function} callback Function to be called to process response.
   */
  function fetchPrinters(callback, all) {
   var query = 'q=^recent';
   if (all) {
     query = '';
   }
   sendCloudPrintRequest('GET',
                         'search',
                         xCloudPrintURLHeader,
                         [query],
                         null,
                         fetchPrintersResponse.bind(this, callback));
  }

  /**
   * Handle the response from printing to cloud print.
   * @param {function} callback Function to be called to process response.
   * @param {XMLHttpRequest} xhr The object used to make the request.
   */
  function printToCloudResponse(callback, xhr) {
    if (xhr.status == 200) {
      var printResult = JSON.parse(xhr.responseText);
      if (printResult['success']) {
        callback.call();
      }
    }
    // TODO(abodenha@chromium.org) Catch and handle failures.
  }

  /**
   * Send the current document to cloud print.
   * @param {string} data The document to be printed.
   * @param {function} callback Function to be called to process response.
   */
  function printToCloud(data, callback) {
    // TODO(abodenha@chromium.org) Make sure we have an XSRF token before
    // sending a submit.  Right now if the user clicks print before we
    // complete any request we wont have an XSRF and the submit will fail.
    sendCloudPrintRequest('POST',
                          'submit',
                          xCloudPrintFormHeader,
                          null,
                          data,
                          printToCloudResponse.bind(this, callback));
  }

  /**
   * Gets the JSON string used to control the behavior of the current
   * print job.
   * @param {Object} printer The printer object to get the ticket for.
   * @return {string} The print ticket or null if not a cloud printer.
   */
  function getPrintTicketJSON(printer) {
    if (isCloudPrint(printer)) {
      return JSON.stringify({'capabilities':
                            [printer.cloudPrintOptions.colorOption]});
    } else {
      return null;
    }
  }

  /**
   * Process the response from cloud print containing the capabilities
   * for the printer.
   * @param {function} callback Function to be called to process response.
   * @param {Object} printer The printer object to get the capabilites for.
   * @param {XMLHttpRequest} xhr The object used to make the request.
   */
  function updatePrinterCapsResponse(callback, printer, xhr) {
    if (xhr.status == 200) {
      var printResult = JSON.parse(xhr.responseText);
      if (printResult['success']) {
        if (!printer.cloudPrintOptions)
          printer.cloudPrintOptions = new Object;
        printer.cloudPrintOptions.capsDownloaded = true;
        printer.cloudPrintOptions.colorOption = null;
        printer.cloudPrintOptions.colorIsDefault = false;
        var detailedCapabilities = printResult.printers[0].capabilities;
        for (var capability in detailedCapabilities) {
          var cap = detailedCapabilities[capability];
          if (cap.name == 'ns1:Colors') {
            printer.cloudPrintOptions.colorOption = new Object;
            printer.cloudPrintOptions.colorOption.name = cap.name;
            printer.cloudPrintOptions.colorOption.type = cap.type;
            for (var option in cap.options) {
              var opt = cap.options[option];
              if (opt.name == 'Color') {
                printer.cloudPrintOptions.colorOnOption = opt;
              }
              if (opt.name == 'Grey_K') {
                printer.cloudPrintOptions.colorOffOption = opt;
              }
              if (opt.default) {
                printer.cloudPrintOptions.colorOption.options = [opt];
                printer.cloudPrintOptions.colorIsDefault =
                    opt.name == printer.cloudPrintOptions.colorOnOption.name;
              }
            }
          }
        }
        callback.call(this, printer);
      }
    }
  }

  /**
   * Retrieve capabilities for a printer.
   * @param {Object} printer The printer object to get the capabilities for.
   * @param {function} callback Function to be called to process response.
   */
  function updatePrinterCaps(printer, callback) {
    if (isCloudPrint(printer) && !printer.cloudPrintOptions.capsDownloaded) {
      sendCloudPrintRequest('GET',
                            'printer?printerid=' +
                                printer.value +
                                '&output=json',
                            xCloudPrintURLHeader,
                            null,
                            null,
                            updatePrinterCapsResponse.bind(this,
                                                           callback,
                                                           printer));
    } else {
      callback.call(this, printer);
    }
  }

  /**
   * @param {Object} printer The printer object to get the data for.
   * @return {boolean} true if the printer supports color.
   */
  function supportsColor(printer) {
    return isCloudPrint(printer) &&
           printer.cloudPrintOptions.colorOption != null;
  }

  /**
   * @param {Object} printer The printer object to get the data for.
   * @return {boolean} true if the printer defaults to color.
   */
  function colorIsDefault(printer) {
    // For now assume that unsupported color means we just don't know
    // and assume color.
    return !supportsColor(printer) ||
        (isCloudPrint(printer) && printer.cloudPrintOptions.colorIsDefault);
  }

  /**
   * Turn color on or off for the specified printer.
   * @param {Object} printer The printer object to turn color on/off for.
   * @param {boolean} color True to turn color on.
   */
  function setColor(printer, color) {
    if (isCloudPrint(printer) && supportsColor(printer)) {
      if (color) {
        printer.cloudPrintOptions.colorOption.options =
            [printer.cloudPrintOptions.colorOnOption];
      } else {
        printer.cloudPrintOptions.colorOption.options =
            [printer.cloudPrintOptions.colorOffOption];
      }
    }
  }

  /**
   * Creates a cloud print printer and sets it as the default printer.
   * @param {string} printer_name The name of the printer to create.
   * @param {Object} cloud_print_data Data to be stored in cloudPrintOptions.
   * @param {function} add_callback The callback to be called to add the new
   *     printer to the print preview UI.
   * @param {function} update_caps_callback The callback to be called to update
   *     capabilities on the new printer.
   */
  function setDefaultPrinter(printer_name,
                             cloud_print_data,
                             add_callback,
                             update_caps_callback) {
    var printer = addCloudPrinters([JSON.parse(cloud_print_data)],
                                   add_callback);
    if (printer)
      update_caps_callback(printer);
  }

  /**
   * Returns the data necessary to serialize a cloud print printer.
   * @param {Object} printer The printer object to get data for.
   * @return {string} A JSON string that can be used to recreate the
   *     cloud print portion of the printer object, or and empty string if
   *     there is no data to save.
   */
  function getData(printer) {
    if (isCloudPrint(printer)) {
      return JSON.stringify(printer.cloudPrintOptions);
    } else {
      return '';
    }
  }

  /**
   * Test if a printer is a cloud print printer.
   * @param {Object} printer The printer to test.
   * @return {boolean} true iff the printer is a cloud print printer.
   */
  function isCloudPrint(printer) {
    return printer && printer.cloudPrintOptions != null;
  }

  /**
   * Mark a printer as a cloud print printer and record its name and id.
   * @param {Object} printer The printer to mark.
   * @param {string} name The user visible name of the printer.
   * @param {string} id The id of the printer used by cloud print to
   *     identify it.
   */
  function setCloudPrint(printer, name, id) {
    if (!printer.cloudPrintOptions) {
      printer.cloudPrintOptions = new Object;
    }
    printer.cloudPrintOptions.name = name;
    printer.cloudPrintOptions.id = id;
  }

  /**
   * Test if a particular cloud printer has already been added to the
   * printer dropdown.
   * @param {string} id A unique value to track this printer.
   * @return {boolean} True if |id| has previously been passed to
   *     trackCloudPrinterAdded.
   */
  function cloudPrinterAlreadyAdded(id) {
    return addedCloudPrinters[id];
  }

  /**
   * Test if a particular printer has already been added to the printers
   * dropdown.  Records it if not.
   * @param {string} id A unique value to track this printer.
   * @return {boolean} False if adding this printer would exceed
   *     |maxCloudPrinters|.
   */
  function trackCloudPrinterAdded(id) {
    if (Object.keys(addedCloudPrinters).length < maxCloudPrinters) {
      addedCloudPrinters[id] = true;
      return true;
    } else {
      return false;
    }
  }

  /**
   * Add cloud printers to the list drop down.
   * Called from the cloudprint object on receipt of printer information from
   * the cloud print server.
   * @param {Array} printers Array of printer info objects.
   * @return {Object} The currently selected printer.
   */
  function addCloudPrinters(printers, addDestinationListOptionAtPosition) {
    var isFirstPass = false;
    var printerList = $('printer-list');

    if (firstCloudPrintOptionPos == lastCloudPrintOptionPos) {
      isFirstPass = true;
      // Remove empty entry added by setDefaultPrinter.
      if (printerList[0] && printerList[0].textContent == '')
        printerList.remove(0);
    }
    if (printers != null) {
      for (var i = 0; i < printers.length; i++) {
        if (!cloudPrinterAlreadyAdded(printers[i]['id'])) {
          if (!trackCloudPrinterAdded(printers[i]['id'])) {
            break;
          }
          var option = addDestinationListOptionAtPosition(
              lastCloudPrintOptionPos++,
              printers[i]['name'],
              printers[i]['id'],
              printers[i]['name'] == defaultOrLastUsedPrinterName,
              false,
              false);
          cloudprint.setCloudPrint(option,
                                   printers[i]['name'],
                                   printers[i]['id']);
        }
      }
    } else {
      if (!cloudPrinterAlreadyAdded(SIGN_IN)) {
        addDestinationListOptionAtPosition(lastCloudPrintOptionPos++,
                                           localStrings.getString('signIn'),
                                           SIGN_IN,
                                           false,
                                           false,
                                           false);
        trackCloudPrinterAdded(SIGN_IN);
        chrome.send('signIn');
      }
    }
    var selectedPrinter = printerList.selectedIndex;
    if (selectedPrinter < 0)
      return null;
    return printerList.options[selectedPrinter];
  }

  return {
    addCloudPrinters: addCloudPrinters,
    colorIsDefault: colorIsDefault,
    fetchPrinters: fetchPrinters,
    getBaseURL: getBaseURL,
    getData: getData,
    getPrintTicketJSON: getPrintTicketJSON,
    isCloudPrint: isCloudPrint,
    printToCloud: printToCloud,
    setBaseURL: setBaseURL,
    setCloudPrint: setCloudPrint,
    setColor: setColor,
    setDefaultPrinter: setDefaultPrinter,
    supportsColor: supportsColor,
    updatePrinterCaps: updatePrinterCaps
  };
});

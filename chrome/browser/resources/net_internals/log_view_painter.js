// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TODO(eroman): This needs better presentation, and cleaner code. This
 *               implementation is more of a transitionary step as
 *               the old net-internals is replaced.
 */

var printLogEntriesAsText;
var proxySettingsToString;
var stripCookiesAndLoginInfo;

// Start of anonymous namespace.
(function() {
'use strict';

function canCollapseBeginWithEnd(beginEntry) {
  return beginEntry &&
         beginEntry.isBegin() &&
         beginEntry.end &&
         beginEntry.end.index == beginEntry.index + 1 &&
         (!beginEntry.orig.params || !beginEntry.end.orig.params);
}

/**
 * Adds a child pre element to the end of |parent|, and writes the
 * formatted contents of |logEntries| to it.
 */
printLogEntriesAsText = function(logEntries, parent, enableSecurityStripping) {
  var entries = LogGroupEntry.createArrayFrom(logEntries);
  var tablePrinter = new TablePrinter();

  if (entries.length == 0)
    return;

  var startDate = timeutil.convertTimeTicksToDate(entries[0].orig.time);
  var startTime = startDate.getTime();

  for (var i = 0; i < entries.length; ++i) {
    var entry = entries[i];

    // Avoid printing the END for a BEGIN that was immediately before, unless
    // both have extra parameters.
    if (!entry.isEnd() || !canCollapseBeginWithEnd(entry.begin)) {
      tablePrinter.addRow();

      tablePrinter.addCell('t=');
      var date = timeutil.convertTimeTicksToDate(entry.orig.time);
      var tCell = tablePrinter.addCell(date.getTime());
      tCell.alignRight = true;
      tablePrinter.addCell(' [st=');
      var stCell = tablePrinter.addCell(date.getTime() - startTime);
      stCell.alignRight = true;
      tablePrinter.addCell('] ');

      for (var j = entry.getDepth(); j > 0; --j)
        tablePrinter.addCell('  ');

      var eventText = getTextForEvent(entry);
      // Get the elapsed time, and append it to the event text.
      if (entry.isBegin()) {
        var dt = '?';
        // Definite time.
        if (entry.end) {
          dt = entry.end.orig.time - entry.orig.time;
        }
        eventText += '  [dt=' + dt + ']';
      }

      var mainCell = tablePrinter.addCell(eventText);
      mainCell.allowOverflow = true;
    }

    // Output the extra parameters.
    if (entry.orig.params != undefined) {
      // Those 5 skipped cells are: two for "t=", and three for "st=".
      tablePrinter.setNewRowCellIndent(5 + entry.getDepth());
      addRowsForExtraParams(tablePrinter,
                            entry.orig,
                            enableSecurityStripping);
      tablePrinter.setNewRowCellIndent(0);
    }
  }

  // Format the table for fixed-width text.
  tablePrinter.toText(0, parent);
}

/**
 * |hexString| must be a string of hexadecimal characters with no whitespace,
 * whose length is a multiple of two.  Returns a string spanning multiple lines,
 * with the hexadecimal characters from |hexString| on the left, in groups of
 * two, and their corresponding ASCII characters on the right.
 *
 * |asciiCharsPerLine| specifies how many ASCII characters will be put on each
 * line of the output string.
 */
function formatHexString(hexString, asciiCharsPerLine) {
  // Number of transferred bytes in a line of output.  Length of a
  // line is roughly 4 times larger.
  var hexCharsPerLine = 2 * asciiCharsPerLine;
  var out = [];
  for (var i = 0; i < hexString.length; i += hexCharsPerLine) {
    var hexLine = '';
    var asciiLine = '';
    for (var j = i; j < i + hexCharsPerLine && j < hexString.length; j += 2) {
      var hex = hexString.substr(j, 2);
      hexLine += hex + ' ';
      var charCode = parseInt(hex, 16);
      // For ASCII codes 32 though 126, display the corresponding
      // characters.  Use a space for nulls, and a period for
      // everything else.
      if (charCode >= 0x20 && charCode <= 0x7E) {
        asciiLine += String.fromCharCode(charCode);
      } else if (charCode == 0x00) {
        asciiLine += ' ';
      } else {
        asciiLine += '.';
      }
    }

    // Max sure the ASCII text on last line of output lines up with previous
    // lines.
    hexLine += makeRepeatedString(' ', 3 * asciiCharsPerLine - hexLine.length);
    out.push('   ' + hexLine + '  ' + asciiLine);
  }
  return out.join('\n');
}

/**
 * Splits |text| in shorter strings around linebreaks.  For each of the
 * resulting strings, adds a row to |tablePrinter| with a cell containing
 * that text, linking to |link|.  |link| may be null.
 */
function addTextRows(tablePrinter, text, link) {
  var textLines = text.split('\n');

  for (var i = 0; i < textLines.length; ++i) {
    tablePrinter.addRow();
    var cell = tablePrinter.addCell(textLines[i]);
    cell.link = link;
    cell.allowOverflow = true;
  }
}

/**
 * Returns a list of FormattedTextInfo objects for |entry|'s |params|.
 */
function addRowsForExtraParams(tablePrinter, entry, enableSecurityStripping) {
  // Format the extra parameters (use a custom formatter for certain types,
  // but default to displaying as JSON).

  // If security stripping is enabled, remove data as needed.
  if (enableSecurityStripping)
    entry = stripCookiesAndLoginInfo(entry);

  switch (entry.type) {
    case LogEventType.HTTP_TRANSACTION_SEND_REQUEST_HEADERS:
    case LogEventType.HTTP_TRANSACTION_SEND_TUNNEL_HEADERS:
      addTextRows(tablePrinter,
                  getTextForRequestHeadersExtraParam(entry),
                  null);
      return;

    case LogEventType.HTTP_TRANSACTION_READ_RESPONSE_HEADERS:
    case LogEventType.HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS:
      addTextRows(tablePrinter,
                  getTextForResponseHeadersExtraParam(entry),
                  null);
      return;

    case LogEventType.PROXY_CONFIG_CHANGED:
      addTextRows(tablePrinter,
                  getTextForProxyConfigChangedExtraParam(entry),
                  null);
      return;

    case LogEventType.CERT_VERIFIER_JOB:
    case LogEventType.SSL_CERTIFICATES_RECEIVED:
      addTextRows(tablePrinter,
                  getTextForCertificatesExtraParam(entry),
                  null);
      return;

    default:
      for (var k in entry.params) {
        if (k == 'headers' && entry.params[k] instanceof Array) {
          addTextRows(tablePrinter,
                      getTextForResponseHeadersExtraParam(entry),
                      null);
          continue;
        }
        var value = entry.params[k];
        // For transferred bytes, display the bytes in hex and ASCII.
        if (k == 'hex_encoded_bytes') {
          addTextRows(tablePrinter, ' --> ' + k + ' =');
          addTextRows(tablePrinter, formatHexString(value, 20));
          continue;
        }

        var paramStr = ' --> ' + k + ' = ';

        // Handle source_dependency entries - add link and map source type to
        // string.
        if (k == 'source_dependency' && typeof value == 'object') {
          var link = '#events&s=' + value.id;
          var sourceType = LogSourceTypeNames[value.type];
          paramStr += value.id + ' (' + sourceType + ')';
          addTextRows(tablePrinter, paramStr, link);
          continue;
        }

        paramStr += JSON.stringify(value);

        // Append the symbolic name for certain constants. (This relies
        // on particular naming of event parameters to infer the type).
        if (typeof value == 'number') {
          if (k == 'net_error') {
            paramStr += ' (' + netErrorToString(value) + ')';
          } else if (k == 'load_flags') {
            paramStr += ' (' + getLoadFlagSymbolicString(value) + ')';
          }
        }

        addTextRows(tablePrinter, paramStr, null);
      }
  }
}

/**
 * Returns the set of LoadFlags that make up the integer |loadFlag|.
 * For example: getLoadFlagSymbolicString(
 */
function getLoadFlagSymbolicString(loadFlag) {
  // Load flag of 0 means "NORMAL". Special case this, since and-ing with
  // 0 is always going to be false.
  if (loadFlag == 0)
    return getKeyWithValue(LoadFlag, loadFlag);

  var matchingLoadFlagNames = [];

  for (var k in LoadFlag) {
    if (loadFlag & LoadFlag[k])
      matchingLoadFlagNames.push(k);
  }

  return matchingLoadFlagNames.join(' | ');
}

/**
 * Indent |lines| by |start|.
 *
 * For example, if |start| = ' -> ' and |lines| = ['line1', 'line2', 'line3']
 * the output will be:
 *
 *   " -> line1\n" +
 *   "    line2\n" +
 *   "    line3"
 */
function indentLines(start, lines) {
  return start + lines.join('\n' + makeRepeatedString(' ', start.length));
}

/**
 * Removes a cookie or unencrypted login information from a single HTTP header
 * line, if present, and returns the modified line.  Otherwise, just returns
 * the original line.
 */
function stripCookieOrLoginInfo(line) {
  var patterns = [
      // Cookie patterns
      /^set-cookie:/i,
      /^set-cookie2:/i,
      /^cookie:/i,

      // Unencrypted authentication patterns
      /^authorization: \S*/i,
      /^proxy-authorization: \S*/i];
  for (var i = 0; i < patterns.length; i++) {
    var match = patterns[i].exec(line);
    if (match != null)
      return match[0] + ' [value was stripped]';
  }

  // Remove authentication information from data received from the server in
  // multi-round Negotiate authentication.
  var challengePatterns = [
      /^www-authenticate: (\S*)\s*/i,
      /^proxy-authenticate: (\S*)\s*/i];
  for (var i = 0; i < challengePatterns.length; i++) {
    var match = challengePatterns[i].exec(line);
    if (!match)
      continue;

    // If there's no data after the scheme name, do nothing.
    if (match[0].length == line.length)
      break;

    // Ignore lines with commas in them, as they may contain lists of schemes,
    // and the information we want to hide is Base64 encoded, so has no commas.
    if (line.indexOf(',') >= 0)
      break;

    // Ignore Basic and Digest authentication challenges, as they contain
    // public information.
    if (/^basic$/i.test(match[1]) || /^digest$/i.test(match[1]))
      break;

    return match[0] + '[value was stripped]';
  }

  return line;
}

/**
 * If |entry| has headers, returns a copy of |entry| with all cookie and
 * unencrypted login text removed.  Otherwise, returns original |entry| object.
 * This is needed so that JSON log dumps can be made without affecting the
 * source data.
 */
stripCookiesAndLoginInfo = function(entry) {
  if (!entry.params || !entry.params.headers ||
      !(entry.params.headers instanceof Array)) {
    return entry;
  }

  // Duplicate the top level object, and |entry.params|.  All other fields are
  // just pointers to the original values, as they won't be modified, other than
  // |entry.params.headers|.
  entry = shallowCloneObject(entry);
  entry.params = shallowCloneObject(entry.params);

  entry.params.headers = entry.params.headers.map(stripCookieOrLoginInfo);
  return entry;
}

function getTextForRequestHeadersExtraParam(entry) {
  var params = entry.params;

  // Strip the trailing CRLF that params.line contains.
  var lineWithoutCRLF = params.line.replace(/\r\n$/g, '');
  return indentLines(' --> ', [lineWithoutCRLF].concat(params.headers));
}

function getTextForResponseHeadersExtraParam(entry) {
  return indentLines(' --> ', entry.params.headers);
}

/*
 * Pretty-prints the contents of an X509CertificateNetLogParam value.
 * @param {LogGroupEntry} A LogGroupEntry for an X509CertificateNetLogParam.
 * @return {string} A formatted string containing all the certificates, in
 *     PEM-encoded form.
 */
function getTextForCertificatesExtraParam(entry) {
  if (!entry.params || !entry.params.certificates) {
    // Some events, such as LogEventType.CERT_VERIFIER_JOB, only log
    // certificates on the begin event.
    return '';
  }

  var certs = entry.params.certificates.reduce(function(previous, current) {
    return previous.concat(current.split('\n'));
  }, new Array());
  return ' --> certificates =\n' + indentLines('        ', certs);
}

function getTextForProxyConfigChangedExtraParam(entry) {
  var params = entry.params;
  var out = '';
  var indentation = '        ';

  if (params.old_config) {
    var oldConfigString = proxySettingsToString(params.old_config);
    // The previous configuration may not be present in the case of
    // the initial proxy settings fetch.
    out += ' --> old_config =\n' +
           indentLines(indentation, oldConfigString.split('\n'));
    out += '\n';
  }

  var newConfigString = proxySettingsToString(params.new_config);
  out += ' --> new_config =\n' +
         indentLines(indentation, newConfigString.split('\n'));

  return out;
}

function getTextForEvent(entry) {
  var text = '';

  if (entry.isBegin() && canCollapseBeginWithEnd(entry)) {
    // Don't prefix with '+' if we are going to collapse the END event.
    text = ' ';
  } else if (entry.isBegin()) {
    text = '+' + text;
  } else if (entry.isEnd()) {
    text = '-' + text;
  } else {
    text = ' ';
  }

  text += LogEventTypeNames[entry.orig.type];
  return text;
}

proxySettingsToString = function(config) {
  if (!config)
    return '';

  // The proxy settings specify up to three major fallback choices
  // (auto-detect, custom pac url, or manual settings).
  // We enumerate these to a list so we can later number them.
  var modes = [];

  // Output any automatic settings.
  if (config.auto_detect)
    modes.push(['Auto-detect']);
  if (config.pac_url)
    modes.push(['PAC script: ' + config.pac_url]);

  // Output any manual settings.
  if (config.single_proxy || config.proxy_per_scheme) {
    var lines = [];

    if (config.single_proxy) {
      lines.push('Proxy server: ' + config.single_proxy);
    } else if (config.proxy_per_scheme) {
      for (var urlScheme in config.proxy_per_scheme) {
        if (urlScheme != 'fallback') {
          lines.push('Proxy server for ' + urlScheme.toUpperCase() + ': ' +
                     config.proxy_per_scheme[urlScheme]);
        }
      }
      if (config.proxy_per_scheme.fallback) {
        lines.push('Proxy server for everything else: ' +
                   config.proxy_per_scheme.fallback);
      }
    }

    // Output any proxy bypass rules.
    if (config.bypass_list) {
      if (config.reverse_bypass) {
        lines.push('Reversed bypass list: ');
      } else {
        lines.push('Bypass list: ');
      }

      for (var i = 0; i < config.bypass_list.length; ++i)
        lines.push('  ' + config.bypass_list[i]);
    }

    modes.push(lines);
  }

  // If we didn't find any proxy settings modes, we are using DIRECT.
  if (modes.length < 1)
    return 'Use DIRECT connections.';

  // If there was just one mode, don't bother numbering it.
  if (modes.length == 1)
    return modes[0].join('\n');

  // Otherwise concatenate all of the modes into a numbered list
  // (which correspond with the fallback order).
  var result = [];
  for (var i = 0; i < modes.length; ++i)
    result.push(indentLines('(' + (i + 1) + ') ', modes[i]));

  return result.join('\n');
};

// End of anonymous namespace.
})();


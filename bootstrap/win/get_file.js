// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function download(sURL, sPath, verbose) {
  if (verbose) {
    WScript.StdOut.Write(" *  GET " + sURL + "...");
  }
	var oResponseBody = null;
	try {
		oXMLHTTP = new ActiveXObject("MSXML2.ServerXMLHTTP");
	} catch (e) {
		WScript.StdOut.WriteLine("[-] XMLHTTP " + new Number(e.number).toHex() + 
			": Cannot create Active-X object (" + e.description) + ").";
		WScript.Quit(1);
	}
	try {
		oXMLHTTP.open("GET", sURL, false);
	} catch (e) {
		WScript.StdOut.WriteLine("[-] XMLHTTP " + new Number(e.number).toHex() + 
			": invalid URL.");
		WScript.Quit(1);
	}
	var sSize = "?";
	var iSize
	try {
		oXMLHTTP.send(null);
		if (oXMLHTTP.status != 200) {
			WScript.StdOut.WriteLine("[-] HTTP " + oXMLHTTP.status + " " + 
				oXMLHTTP.statusText);
			WScript.Quit(1);
		}
		oResponseBody = oXMLHTTP.responseBody;
		sSize = oXMLHTTP.getResponseHeader("Content-Length");
		if (sSize != "") {
			iSize = parseInt(sSize)
			sSize = iSize.toBytes();
		} else {
			try {
				iSize = new Number(oXMLHTTP.responseText.length)
				sSize = iSize.toBytes();
			} catch(e) {
				sSize = "unknown size";
			}
		}
	} catch (e) {
		WScript.StdOut.WriteLine("[-] XMLHTTP " + new Number(e.number).toHex() + 
			": Cannot make HTTP request (" + e.description) + ")";
		WScript.Quit(1);
	}

  if (verbose) {
    WScript.StdOut.WriteLine("ok (" + sSize + ").");
    WScript.StdOut.Write(" *  Save " + sPath + "...");
  }

	try {
		var oAS = new ActiveXObject("ADODB.Stream");
		oAS.Mode = 3; // ReadWrite
		oAS.Type = 1; // 1= Binary
		oAS.Open(); // Open the stream
		oAS.Write(oResponseBody); // Write the data
		oAS.SaveToFile(sPath, 2); // Save to our destination
		oAS.Close();
	} catch(e) {
		WScript.StdOut.WriteLine("[-] ADODB.Stream " + new Number(e.number).toHex() + 
			": Cannot save file (" + e.description + ")");
		WScript.Quit(1);
	}
	if (typeof(iSize) != undefined) {
		oFSO = WScript.CreateObject("Scripting.FileSystemObject")
		oFile = oFSO.GetFile(sPath)
		if (oFile.Size < iSize) {
			WScript.StdOut.WriteLine("[-] File only partially downloaded.");
			WScript.Quit(1);
		}
	}
  if (verbose) {
    WScript.StdOut.WriteLine("ok.");
  }
}

Number.prototype.isInt = function Number_isInt() {
	return this % 1 == 0; 
};
Number.prototype.toBytes = function Number_toBytes() {
	// Returns a "pretty" string representation of a number of bytes:
	var aUnits = ["KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"];
	var sUnit = "bytes";
	var iLimit = 1; 
	while(this > iLimit * 1100 && aUnits.length > 0) {
		iLimit *= 1024;
		sUnit = aUnits.shift();
	}
	return (Math.round(this * 100 / iLimit) / 100).toString() + " " + sUnit;
};
Number.prototype.toHex = function Number_toHex(nLength) {
	if (arguments.length == 0) nLength = 1;
	if (typeof(nLength) != "number" && !(nLength instanceof Number)) {
		throw Exception("Length must be a positive integer larger than 0.", TypeError, 0);
	}
	if (nLength < 1 || !nLength.isInt()) {
		throw Exception("Length must be a positive integer larger than 0.", "RangeError", 0);
	}
	var sResult = (this + (this < 0 ? 0x100000000 : 0)).toString(16);
	while (sResult.length < nLength) sResult = "0" + sResult;
	return sResult;
};

if (WScript.Arguments.length != 2) {
  WScript.StdOut.Write("Incorrect arguments to download.js")
} else {
  download(WScript.Arguments(0), WScript.Arguments(1), false);
}

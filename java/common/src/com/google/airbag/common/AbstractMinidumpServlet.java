/* Copyright (C) 2006 Google Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

package com.google.airbag.common;

import java.io.IOException;
import java.io.InputStream;
import java.text.NumberFormat;
import java.util.Calendar;
import java.util.Random;
import java.util.SortedMap;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

/**
 * Common part of uploading a file with parameters. A subclass needs to 
 * provide a ReportStorage implementation.
 *
 * An upload file is saved in the file storage with parsed parameters from
 * the URL.
 *
 * A separate processor will access the file storage and process these
 * reports.
 */
@SuppressWarnings("serial")
public class AbstractMinidumpServlet extends HttpServlet {
  // Minidump storage to be instantiated by subclasses in init() method.
  protected ReportStorage minidumpStorage;

  // Random number generator
  private final Random random = new Random();
  
  /**
   * Always return success to a GET, to keep out nosy people who go
   * directly to the URL.
   */
  public void doGet(HttpServletRequest req, HttpServletResponse res) {
    res.setStatus(HttpServletResponse.SC_OK);
  }

  /**
   * Takes the file POSTed to the server and writes it to a file storage.
   * Parameters in URL are saved as attributes of the file.
   *
   * @param req a wrapped HttpServletRequest that represents a multipart 
   *            request
   * @return unique ID for this report, can be used to get parameters and
   *         uploaded file contents from a file storage. If these is a 
   *         collation, returns null.
   *         
   * @throws ServletException
   * @throws IOException
   */
  protected String saveFile(MultipartRequest req)
      throws ServletException, IOException {
    // parse mutilpart request 
    SortedMap<String, String> params = req.getParameters();

    //TODO(fqian): verify required fields of a report
    InputStream inputs = req.getInputStream();

    /* It is possible that two or more clients report crashes at the same
     * time with same parameters. To reduce the chance of collation, we
     * add two internal parameters:
     * 1. reporting time, a time string in the form of YYMMDD-HHMMSS;
     * 2. a random number;
     *
     * In theory, there is still a chance to collate, but it is very low.
     * When collation happens, the one coming later is dropped.
     */
    // 1. add a timestamp to parameters
    params.put(NameConstants.REPORTTIME_PNAME, currentTimeString());
    
    // 2. get a random number to make the change of collation very small
    int r;
    synchronized (this.random) {
      r = this.random.nextInt();
    }
    params.put(NameConstants.RANDOMNUM_PNAME, Integer.toString(r));
    
    String fid = this.minidumpStorage.getUniqueId(params);
    
    assert fid != null;
    
    if (this.minidumpStorage.reportExists(fid)) {
      // collation happens 
      return null;
    }
    
    this.minidumpStorage.saveAttributes(fid, params);
    // save uploaded contents to the storage
    this.minidumpStorage.writeStreamToReport(fid, inputs, 0);
    
    return fid;
  }

  /* Gets a string representing the current time using the format: 
   * YYMMDD-HHMMSS.
   */
  private String currentTimeString() {
    NumberFormat formatter = NumberFormat.getInstance();
    formatter.setGroupingUsed(false);
    formatter.setMinimumIntegerDigits(2);  // 2 digits per date component

    // All servers are in Pacific time
    Calendar cal = Calendar.getInstance();

    StringBuffer tstring = new StringBuffer();
    tstring.append(formatter.format(cal.get(Calendar.YEAR)));
    // We want January = 1.
    tstring.append(formatter.format((cal.get(Calendar.MONTH) + 1)));
    tstring.append(formatter.format(cal.get(Calendar.DAY_OF_MONTH)));
    tstring.append("-");
    tstring.append(formatter.format(cal.get(Calendar.HOUR_OF_DAY)));
    tstring.append(formatter.format(cal.get(Calendar.MINUTE)));
    tstring.append(formatter.format(cal.get(Calendar.SECOND)));
    
    return new String(tstring);
  }
}

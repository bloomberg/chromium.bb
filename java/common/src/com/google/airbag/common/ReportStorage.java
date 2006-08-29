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

import java.io.InputStream;
import java.io.IOException;
import java.io.FileNotFoundException;

import java.util.SortedMap;

/**
 * ReportStorage.java
 * 
 * <p>
 * Provide an abstract layer for storing crash reports and associated meta 
 * data.
 * </p>
 * 
 * <p>
 * The interface is intended to be used by a client in the following way:
 * 
 * <pre>
 *     ReportStorage rs = new ReportStorageImpl(...);
 *     // Write an uploading file to a storage
 *     try {
 *        String rid = rs.getUniqueId(attributes); // params from URL
 *        rs.saveAttributes(id, attributes);
 *        if (!rs.reportExists(rid) || allowOverwrite)
 *           rs.writeStreamToReport(rid, input, 0);
 *        else
 *           rs.appendStreamToReport(rid, input, 0);
 *     } catch (...)
 *  
 *     // Read a file from the storage
 *     try {
 *        OutputStream os = fs.openReportForRead(fid);
 *        os.read(...);
 *        os.close();
 *     } catch (...)
 * </pre>
 */

public interface ReportStorage {
  /**
   * Given a sorted map of attributes (name and value), returns a unique id
   * of the crash report.
   * 
   * @param params a sorted map from name to value
   * @return a string as the file id
   */
  public String getUniqueId(SortedMap<String, String> params);

  /**
   * Given a report id, checks if the report data AND attributes identified
   * by this id exists on the storage.
   * 
   * @param id a report id
   * @return true if the id represents an existing file
   */
  public boolean reportExists(String id);

  /**
   * Given a report id and a sorted map of attributes, saves attributes on
   * the storage.
   * 
   * @param id a report id
   * @param attrs attributes associated with this id
   * @return true if attributes are saved successfully
   */
  public boolean saveAttributes(String id, SortedMap<String, String> attrs);

  /**
   * Given a report id, returns attributes associated with this report.
   * 
   * @param id a report id
   * @return a sorted map from name to value
   * @throws FileNotFoundException if fileExists(id) returns false
   */
  public SortedMap<String, String> getAttributes(String id)
      throws FileNotFoundException;

  /**
   * Writes <i>max</i> bytes from an input stream to a report identified by 
   * an <i>id</i>.
   * 
   * @param id a report id
   * @param input an input stream
   * @param max
   *          maximum bytes to be written, if max is less or equal than 0, it will
   *          write all bytes from input stream to the file
   * @return the number of bytes written
   * @throws FileNotFoundException
   *           if reportExists(id) returns false
   * @throws IOException
   */
  public int writeStreamToReport(String id, InputStream input, int max)
      throws FileNotFoundException, IOException;

  /**
   * Opens a report for read.
   * 
   * @param id a report id
   * @return an output stream for read
   * @throws FileNotFoundException
   */
  public InputStream openReportForRead(String id) throws FileNotFoundException;

  /** 
   * @param id a report id
   * @return the checksum of a report.
   * @throws FileNotFoundException 
   */
  public String getChecksum(String id) throws FileNotFoundException;
  
  /**
   * Removes a report.
   * 
   * @param id a report id
   * @return true if the report is removed successfully
   */
  public boolean removeReport(String id) throws FileNotFoundException;
}

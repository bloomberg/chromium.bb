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

import java.util.SortedMap;
import java.util.logging.Logger;

import java.io.File;
import java.io.InputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.FileNotFoundException;

/**
 * <p>Implement FileStorage using a local file system.</p>
 * 
 * <p>Given a sorted map of attributes, create a checksum as unique file 
 * id.</p>
 * 
 * <p>Each file id is associated with two files in the storage:
 *   <ol>
 *    <li>an attribute file named as <id>.attr;</li>
 *    <li>a data file named as <id>.data;</li>
 *   </ol>
 * </p>
 */

public class ReportStorageLocalFileImpl implements ReportStorage {

  // Name extension of attribute file
  private static final String ATTR_FILE_EXT = ".attr";

  // Name extension of data file
  private static final String DATA_FILE_EXT = ".data";

  // Set the maximum file length at 1M
  private static final long MAX_ATTR_FILE_LENGTH = 1 << 10; 

  // Logging
  private static final Logger logger = 
    Logger.getLogger(ReportStorageLocalFileImpl.class.getName());
  
  // Directory name for storing files.
  private String directoryName;

  /** 
   * Creates an instance of ReportStorageLocalFileImpl by providing a 
   * directory name.
   * 
   * @param dirname a directory for storing files
   * @throws IOException
   * @throws NullPointerException if dirname is null
   */
  public ReportStorageLocalFileImpl(String dirname) throws IOException {
    this.directoryName = dirname;
    // new File can throw NullPointerException if dirname is null
    File dir = new File(dirname);
    if (!dir.exists() && !dir.mkdirs())
      throw new IOException("Cannot make dirs for "+dirname);
    
    if (!dir.canWrite())
      throw new IOException("Cannot write to "+dirname
                            +", check your permissions.");
  }
  
  /**
   * Returns a hashed string of attributes. Attributes are saved 
   * in the file storage if not exists.
   */
  public String getUniqueId(SortedMap<String, String> attributes) 
  {
    String attr = CrashUtils.attributesToString(attributes);
    return CrashUtils.dataSignature(attr.getBytes());
  }
  
  /**
   * Saves attributes associated with a given id. if attributes does not 
   * match the id (comparing results of getUniqueId(attributes)
   * with id), it returns false. Otherwise, attributes are saved.
   */
  public boolean saveAttributes(String id, SortedMap<String, String> attributes)
  {
    String attr = CrashUtils.attributesToString(attributes);
    String digest = CrashUtils.dataSignature(attr.getBytes());
    
    if (!digest.equals(id))
      return false;
    
    try {
      File attrFile = new File(this.directoryName, digest+ATTR_FILE_EXT);

      // check if attr file exists
      if (!attrFile.exists()) {
        FileOutputStream fos = new FileOutputStream(attrFile);
        fos.write(attr.getBytes());
        fos.close();
      }

      return true;
    } catch (IOException e) {
      e.printStackTrace();
      logger.warning(e.toString());
      return false;
    }    
  }

  /** Checks if a report id exists. */
  public boolean reportExists(String id) {
    File datafile = new File(this.directoryName, id+DATA_FILE_EXT);
    File attrfile = new File(this.directoryName, id+ATTR_FILE_EXT);
    return datafile.isFile() && attrfile.isFile();
  }

  /** Returns attributes in a map associated with an id. */
  public SortedMap<String, String> getAttributes(String id) 
    throws FileNotFoundException
  {
    if (!this.reportExists(id))
      throw new FileNotFoundException("no file is identified by "+id);
    
    File attrfile = new File(this.directoryName, id+ATTR_FILE_EXT);
    if (!attrfile.isFile())
      throw new FileNotFoundException("no file is identified by "+id);
    
    int length = (int) attrfile.length();
    if (length >= MAX_ATTR_FILE_LENGTH)
      throw new FileNotFoundException("no file is identified by "+id);

    byte[] content = new byte[length];

    try {
      FileInputStream fis = new FileInputStream(attrfile);
      fis.read(content);
      fis.close();

      // verify checksum
      String sig = CrashUtils.dataSignature(content);
      if (!sig.equals(id)) {
        logger.warning("illegal access to "+attrfile);
        return null;
      }

      // parse contents
      return CrashUtils.stringToAttributes(new String(content));

    } catch (IOException e) {
      logger.warning(e.toString());
      return null;
    }
  }

  public int writeStreamToReport(String id, InputStream input, int max) 
    throws FileNotFoundException, IOException {
    File datafile = new File(this.directoryName, id + DATA_FILE_EXT);
    FileOutputStream fos = new FileOutputStream(datafile);

    int bytesCopied = CrashUtils.copyStream(input, fos, max);

    fos.close();
    
    return bytesCopied;
  }

  public InputStream openReportForRead(String id) throws FileNotFoundException {
    File datafile = new File(this.directoryName, id + DATA_FILE_EXT);
    return new FileInputStream(datafile);
  }

  public String getChecksum(String id) throws FileNotFoundException {
    File datafile = new File(this.directoryName, id + DATA_FILE_EXT);
    return CrashUtils.fileSignature(datafile);
  }


  public boolean removeReport(String id) {
    File datafile = new File(this.directoryName, id + DATA_FILE_EXT);
    File attrfile = new File(this.directoryName, id + ATTR_FILE_EXT);
    
    datafile.delete();
    attrfile.delete();
    
    return true;
  }
}

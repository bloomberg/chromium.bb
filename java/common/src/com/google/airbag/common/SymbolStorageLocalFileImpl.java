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

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.SortedMap;
import java.util.TreeMap;

/**
 * Implementation of SymbolStorage interface using a local file system.
 * 
 * Given a set of parameters, prod, ver, app, plat, computes its MD5 digest.
 * Digest + .attr contains attributes and checksum of the symbol file.
 * Symbol file content is stored in a file named by its checksum + .data.
 * 
 * To upload a symbol file, a client can send a query:
 * <pre>
 *   /symexists?sum=<checksum>, the server checks if a symbol file identified by checksum exists;
 *   /upload?prod=<product>&ver=<version>&plat=<platform>&app=<module>&sum=<checksum> use POST
 *   method with or without uploading a file.
 *   
 *   A client can always call /upload to upload a symbol file. 
 *   However, a more efficient way is to check whether a file is on the server by calling /symexists.
 *   If so, the client can just POST the request without actually upload the file content,
 *   checksum is sufficient.
 * 
 *   /getchecksum?prod=<product>&ver=<version>&plat=<platform>&app=<module>, returns the checksum
 *   of the symbol file on the server.
 *   /download?prod=<product>&ver=<version>&plat=<platform>&app=<module>, downloads the symbol file
 *   on the server.
 *   
 *   A client can always use /download to download a symbol file.
 *   However, if the client maintins its own cache of symbol files, it can call /getchecksum,
 *   and look up the cache using the checksum. If the cache does not have the file, then it 
 *   calls /download.
 * </pre>
 */

public class SymbolStorageLocalFileImpl implements SymbolStorage {

  // Name extension of attribute file
  private static final String ATTR_FILE_EXT = ".attr";

  // Name extension of data file
  private static final String DATA_FILE_EXT = ".data";

  private static final int MAX_ATTR_FILE_LENGTH = 1 << 10;
  
  // Directory name for storing files.
  private String directoryName;

  /** 
   * Creates an instance of ReportStorageLocalFileImpl by providing a 
   * directory name.
   * 
   * @param dirname
   * @throws IOException
   */
  public SymbolStorageLocalFileImpl(String dirname) throws IOException {
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
   * Save a symbol file.
   */
  public boolean saveSymbolFile(SortedMap<String, String> attrs, InputStream contents) 
    throws IOException {
    String digest = getAttributesSignature(attrs);
      
    // get 'sum' value
    String checksum = attrs.get(NameConstants.CHECKSUM_PNAME);
    if (checksum == null)
      return false;
 
    // attribute file name and data file name
    File attrFile = new File(this.directoryName, digest+ATTR_FILE_EXT);
    
    // use passed in checksum as file name
    File dataFile = new File(this.directoryName, checksum+DATA_FILE_EXT);
    
    // write data to file
    FileOutputStream outs = new FileOutputStream(dataFile);
    CrashUtils.copyStream(contents, outs, 0);
    outs.close();
     
    // get signature of input stream
    String filesig = CrashUtils.fileSignature(dataFile);
    
    if (!checksum.equals(filesig)) {
      dataFile.delete();
      return false;
    }
    
    // save all attributes with checksum
    String fullAttrs = CrashUtils.attributesToString(attrs);
    
    FileOutputStream fos = new FileOutputStream(attrFile);
    fos.write(fullAttrs.getBytes());
    fos.close();
    
    return true;
  }
  
  
  public String getFileChecksum(SortedMap<String, String> attrs) throws IOException {
    String digest = getAttributesSignature(attrs);
    File attrFile = new File(this.directoryName, digest+ATTR_FILE_EXT);
    
    if (!attrFile.isFile())
      throw new FileNotFoundException();
    
    int length = (int) attrFile.length();
    if (length >= MAX_ATTR_FILE_LENGTH)
      throw new FileNotFoundException();

    byte[] content = new byte[length];

    FileInputStream fis = new FileInputStream(attrFile);
    fis.read(content);
    fis.close();

    // parse contents
    SortedMap<String, String> savedAttrs = 
        CrashUtils.stringToAttributes(new String(content));
    
    return savedAttrs.get(NameConstants.CHECKSUM_PNAME);
  }
  
  public boolean fileExists(String checksum) {
    File dataFile = new File(this.directoryName, checksum+DATA_FILE_EXT); 
    return dataFile.isFile();
  }
  
  public InputStream openFileForRead(SortedMap<String, String> attrs) 
    throws IOException {
    String checksum = getFileChecksum(attrs);
    if (checksum == null)
      throw new FileNotFoundException();
    
    File dataFile = new File(this.directoryName, checksum + DATA_FILE_EXT);
    if (!dataFile.isFile())
      throw new FileNotFoundException();
    
    return new FileInputStream(dataFile);
  }
  
  private static final String[] requiredParameters = {
    NameConstants.PRODUCT_PNAME,
    NameConstants.APPLICATION_PNAME,
    NameConstants.PLATFORM_PNAME,
    NameConstants.VERSION_PNAME
  };
  
  private String getAttributesSignature(SortedMap<String, String> attrs) {
    // canonize parameters
    SortedMap<String, String> params = canonizeAttributes(attrs);
    String attrString = CrashUtils.attributesToString(params);
    return CrashUtils.dataSignature(attrString.getBytes());
  }
  /* Canonize attributes, get 'prod', 'ver', 'plat', and 'app' values,
   * and put them in a new sorted map. If one of value is missing, 
   * returns null.
   */
  private SortedMap<String, String> 
    canonizeAttributes(SortedMap<String, String> attrs) {
    SortedMap<String, String> params = new TreeMap<String, String>();
    for (String s : requiredParameters) {
      String v = attrs.get(s);
      if (v == null)
        return null;
      else 
        params.put(s, v);
    }
    return params; 
  }

}

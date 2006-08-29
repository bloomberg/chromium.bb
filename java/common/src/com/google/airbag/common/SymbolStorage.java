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
import java.util.SortedMap;

/**
 * <p>SymbolStorage provides a simple interface for storing and retrieving 
 * symbol files. Symbol files are indexed by a set of attributes:
 * <ul>
 *   <li>product name</li>
 *   <li>version (build id)</li>
 *   <li>platform</li>
 *   <li>application/module name</li>
 * </ul>
 * </p>
 * 
 * <p>
 * The interface is designed for a symbol server supports upload, getid, 
 * download operations.
 * </p>
 */

public interface SymbolStorage {
  /**
   * Saves a symbol file indexed by a set of attributes. Attributes include
   * product, version, platform, application/module, plus a checksum of
   * the symbol file.
   * 
   * If a symbol file whose checksum matches the attribute, the input stream 
   * can be NULL. No contents will be written. This can save some workloads
   * of uploading symbols.
   * 
   * @param attrs a map of attributes, it must have 'prod', 'ver', 'plat', 
   *        'app', and 'sum', values of the first four attributes are used 
   *        as index, and the value of the last attribute is the MD5 checksum 
   *        of the symbol file for verification.
   * @param contents symbol file contents.
   * @return true if checksum matches 
   * @throws IOException 
   */
  public boolean saveSymbolFile(SortedMap<String, String> attrs, 
      InputStream contents) throws IOException;
  
  /**
   * Gets the checksum of a symbol file indexed by a set of attributes.
   * @param attrs a map of attributes, must include 'prod', 'ver', 'plat', 
   *        and 'app'.
   * @return MD5 checksum as a string
   * @throws IOException 
   */
  public String getFileChecksum(SortedMap<String, String> attrs)
     throws IOException;
  
  /**
   * Checks if a file exists already (identified by its checksum).
   * @param checksum the file checksum
   * @return true if a file with the same checksum exists
   */
  public boolean fileExists(String checksum);
  
  /**
   * Gets an input stream of a symbol server indexed by a set of attributes.
   * @param attrs a map of attributes, must include 'prod', 'ver', 'plat', 
   *        and 'app'.
   * @return an input stream of the symbol file
   * @throws IOException 
   */
  public InputStream openFileForRead(SortedMap<String, String> attrs) 
    throws IOException;
}

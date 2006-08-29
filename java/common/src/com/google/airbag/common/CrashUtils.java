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
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Map;
import java.util.SortedMap;
import java.util.TreeMap;

/** A utility class used by crash reporting server. */

public class CrashUtils {
  // A map from numbers to hexadecimal characters.
  private static final char[] maps = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E', 'F',
  };
  
  private static final String MD_ALGORITHM = "MD5";

  // delimiter between record
  private static final String RECORD_DELIMITER = ";";
  private static final String FIELD_DELIMITER = " ";
  private static final int BUFFER_SIZE = 1024;
  
  /**
   * Given a byte array, returns a string of its hex representation.
   * Each byte is represented by two characters for its high and low
   * parts. For example, if the input is [0x3F, 0x01], this method
   * returns 3F01.
   * 
   * @param bs a byte array
   * @return a string of hexadecimal characters
   */
  public static String bytesToHexString(byte[] bs) {
    StringBuffer sb = new StringBuffer();
    for (byte b : bs) {
      int high = (b >> 4) & 0x0F;
      int low = b & 0x0F;
      sb.append(maps[high]);
      sb.append(maps[low]);
    }
    return sb.toString();
  }
  
  /**
   * Given a byte array, computes its message digest using one-way hash 
   * functions. 
   * 
   * @param data a byte array 
   * @return a string as its signature, or null if no message digest 
   *         algorithm
   * supported by the system.
   */
  public static String dataSignature(byte[] data) {
    try {
      MessageDigest md = MessageDigest.getInstance(MD_ALGORITHM);
      return bytesToHexString(md.digest(data));
    } catch (NoSuchAlgorithmException e) {
      return null;
    }
  }
  
  /** 
   * Compute the signature of a file by calling dataSignature on file
   * contents.
   * 
   * @param file a file name which signature to be computed
   * @return the method signature of the file, or null if failed to 
   * read file contents, or message digest algorithm is not supported
   */
  public static String fileSignature(File file) {
    try {
      FileInputStream fis = new FileInputStream(file);
      byte[] buf = new byte[BUFFER_SIZE];
      MessageDigest md = MessageDigest.getInstance(MD_ALGORITHM);
      while (true) {
        int bytesRead = fis.read(buf, 0, BUFFER_SIZE);
        if (bytesRead == -1)
          break;
        md.update(buf, 0, bytesRead);
      }
      return bytesToHexString(md.digest());
      
    } catch (NoSuchAlgorithmException e) {
      return null;
    } catch (IOException e) {
      return null;
    }
  }
  
  /**
   * Encodes an attribute map to a string. Encoded string format:
   * name value1[ value2];name value1[ value2]
   * Names and values should be escaped so that there are no
   * RECORD_DELIMITER and VALUE_DELIMITER in strings.
   * 
   * @param attributes a maps of attributes name and value to be encoded
   * @return a string of encoded attributes
   */
  public static 
  String attributesToString(SortedMap<String, String> attributes) {
    StringBuffer res = new StringBuffer();
    for (Map.Entry<String, String> e : attributes.entrySet()) {
      String name = e.getKey();
      String value = e.getValue();

      assert name.indexOf(RECORD_DELIMITER) == -1;
      assert name.indexOf(FIELD_DELIMITER) == -1;
      res.append(name).append(FIELD_DELIMITER);

      assert value.indexOf(RECORD_DELIMITER) == -1;
      assert value.indexOf(FIELD_DELIMITER) == -1;
      res.append(value).append(RECORD_DELIMITER);
    }
    return new String(res);
  }

  /**
   *  Decodes a string to a map of attributes. 
   */
  public static SortedMap<String, String> stringToAttributes(String s) {
    SortedMap<String, String> map = 
      new TreeMap<String, String>();
    String[] records = s.split(RECORD_DELIMITER);
    for (String r : records) {
      String[] fields = r.trim().split(FIELD_DELIMITER);
      if (fields.length != 2) // discard records that has no values
        continue;
      String name = fields[0].trim();
      String value = fields[1].trim();
      map.put(name, value);
    }
    return map;
  }
  
  /**
   * Copies bytes from an input stream to an output stream, with max bytes.
   * 
   * @param ins an input stream to read
   * @param outs an output stream to write
   * @param max the maximum number of bytes to copy. If max <= 0, copy bytes 
   *        until the end of input stream
   * @return the number of bytes copied
   * @throws IOException 
   */
  public static int copyStream(InputStream ins, OutputStream outs, int max) 
    throws IOException {
    byte[] buf = new byte[BUFFER_SIZE];
    int bytesWritten = 0;

    while (true) {
      int bytesToRead = BUFFER_SIZE;
      if (max > 0)
        bytesToRead = Math.min(BUFFER_SIZE, max - bytesWritten);

      if (bytesToRead <= 0)
        break;

      int bytesRead = ins.read(buf, 0, bytesToRead);
      if (bytesRead == -1) // end of input stream
        break;
      outs.write(buf, 0, bytesRead);
      bytesWritten += bytesRead;
    }
    
    return bytesWritten;
  }
}

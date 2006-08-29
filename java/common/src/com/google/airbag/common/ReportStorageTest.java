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

import java.io.ByteArrayInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.util.TreeMap;

/** A simple regression test of ReportStorage.java and implementations.
 */

public class ReportStorageTest {
  public static void main(String[] args) {   
    // use /tmp/test as testing directory
    ReportStorage rs = null;
    try {
      rs = new ReportStorageLocalFileImpl("/tmp/test");
    } catch (IOException e) {
      e.printStackTrace();
      System.exit(1);
    }
    
    runTest(rs);
    
    // test passed
    System.out.println("OK.");
  }
  
  private static void runTest(ReportStorage rs) {
    // test CrashUtil.bytesToHexString
    byte[] ba = new byte[4];
    ba[0] = (byte)0xFF;
    ba[1] = (byte)0x00;
    ba[2] = (byte)0x0F;
    ba[3] = (byte)0xF0;
    String s = CrashUtils.bytesToHexString(ba);
    assert s.equals("FF000FF0");
  
    // construct a simple map of attributes
    TreeMap<String, String> params = 
      new TreeMap<String, String>();
    params.put("Hello", "World");
    String rid = rs.getUniqueId(params);
    assert rid != null;
    
    boolean b = rs.saveAttributes(rid, params);
    assert b;
    ba = "hellow, world!".getBytes();
    InputStream in = new ByteArrayInputStream(ba);
    
    assert rs.reportExists(rid);
    
    // save contents to storage
    try {
      rs.writeStreamToReport(rid, in, 0);
    } catch (FileNotFoundException e) {
      e.printStackTrace();
      System.exit(1);
    } catch (IOException e) {
      e.printStackTrace();
      System.exit(1);
    }
    
    // read contents out
    try {
      InputStream in1 = rs.openReportForRead(rid);
      assert in1.available() == ba.length;
      in1.read(ba);
      assert(new String(ba).equals("hellow, world!"));
    } catch (IOException e) { 
      e.printStackTrace();
      System.exit(1);
    }
  }
}

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

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.util.TreeMap;

/**
 * Test ReportStorage and its implementation by simulating two pocesses
 * as producer and consuer.
 * 
 * To use this test with TestReportStorageProducer:
 * > java TestReportStorageProducer /tmp/testdir
 * 
 * In another console,
 * > java TestReportStorageConsumer /tmp/testdir
 * 
 * Then watch output on both console.
 */
public class TestReportStorageConsumer {

  /**
   * @param args
   */
  public static void main(String[] args) {
    String testdir = args[0];
    ReportStorage rs = null;
    try {
      rs = new ReportStorageLocalFileImpl(testdir);
    } catch (IOException e) {
      e.printStackTrace();
      System.exit(1);
    }
    
    consume(rs);
  }
  
  private static void consume(ReportStorage rs) {
    int i = 0;
    TreeMap<String, String> params = 
      new TreeMap<String, String>();
    String v = "hello";
    byte[] buf = new byte[1024];
    while (true) {
      params.put(Integer.toString(i), v);
      String id = rs.getUniqueId(params);
      rs.saveAttributes(id, params);
      if (rs.reportExists(id)) {
        InputStream is = null;
        try {
          is = rs.openReportForRead(id);
          while (is.read(buf) != -1) {
            System.out.print(new String(buf));
          }
        } catch (FileNotFoundException e) {
          e.printStackTrace();
          break;
        } catch (IOException e) {
          e.printStackTrace();
          break;
        }
      } 
      i++;
      try {
        Thread.sleep(1000);
      } catch (InterruptedException e) {
        e.printStackTrace();
        break;
      }
    }
  }
}

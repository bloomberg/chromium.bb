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
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.channels.FileChannel;
import java.nio.channels.FileLock;
import java.util.LinkedList;
import java.util.logging.Logger;

/**
 * Implementation of ReportQueue using a shared file. File accesses are 
 * protected by a file lock. When dequeue, all reports in the file were 
 * returned and the file is truncated to length zero.
 */

public class ReportQueueFileImpl implements ReportQueue {
  private static final Logger logger = 
    Logger.getLogger(ReportQueueFileImpl.class.getName());
  private String queueFile;
  
  /** Given a file name, creates an instance of ReportQueueFileImpl. */
  public ReportQueueFileImpl(String fname) {
    this.queueFile = fname;
  }
  
  /** Enqueues a report id. */
  public boolean enqueue(String rid) {
    try {
      RandomAccessFile raf = new RandomAccessFile(this.queueFile, "rw"); 
      FileChannel fc = raf.getChannel();
      // block thread until lock is obtained
      FileLock lock = fc.lock();
      raf.seek(raf.length());
      raf.writeBytes(rid);
      raf.writeByte('\n');
      lock.release();
      fc.close();
      raf.close();
      return true;
    } catch (FileNotFoundException e) {
      logger.severe("Cannot open file "+this.queueFile+" for write.");
    } catch (IOException e) {
      logger.severe("Cannot write to file "+this.queueFile);
    }
    return false;
  }
  
  /** Enqueues a list of report ids. */
  public boolean enqueue(LinkedList<String> ids) {
    try {
      RandomAccessFile raf = new RandomAccessFile(this.queueFile, "rw"); 
      FileChannel fc = raf.getChannel();
      // block thread until lock is obtained
      FileLock lock = fc.lock();
      raf.seek(raf.length());
      for (String rid : ids) {
        raf.writeBytes(rid);
        raf.writeByte('\n');
      }
      lock.release();
      fc.close();
      raf.close();
      return true;
    } catch (FileNotFoundException e) {
      logger.severe("Cannot open file "+this.queueFile+" for write.");
    } catch (IOException e) {
      logger.severe("Cannot write to file "+this.queueFile);
    }
    return false;
  }

  public boolean empty() {
    // check the length of the file
    File f = new File(this.queueFile);
    return f.length() == 0L;
  }

  public LinkedList<String> dequeue() {
    LinkedList<String> ids = new LinkedList<String>();
    
    try {
      RandomAccessFile raf = new RandomAccessFile(this.queueFile, "rw");
      FileChannel fc = raf.getChannel();
      FileLock flock = fc.lock();
       
      while (true) {
        String s = raf.readLine();
        if (s == null)
          break;
        s = s.trim();
        if (s.equals(""))
          continue;
        
        ids.add(s);
      }
    
      fc.truncate(0L);
    
      // release the lock
      flock.release();
      fc.close();
      raf.close();
    } catch (FileNotFoundException e) {
      logger.severe("Cannot open file "+this.queueFile+" for write.");
    } catch (IOException e) {
      logger.severe("Cannot write to file "+this.queueFile);
    }
    return ids;
  }
}

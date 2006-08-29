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

import java.util.LinkedList;
import java.io.File;
import java.io.IOException;

/**
 * Implements ReportQueue using directories. Some restrictions:
 * <ul>
 *   <li>Ids must be valid file names;</li>
 *   <li>Ids cannot be duplicated, a duplicated id is ignored;</li>
 *   <li>No guarantees on ordering, in other words, this is not really 
 *       a queue (with FIFO order);</li>
 * </ul>
 */

public class ReportQueueDirImpl implements ReportQueue {
  // maximum number of ids returned by dequque method.
  private static final int MAX_DEQUEUED_IDS = 100;
  
  // the directory name for storing files
  private String queueDir;
  
  /**
   * Creates an instance of ReportQueueDirImpl with a directory name.
   * @param dirname
   */
  public ReportQueueDirImpl(String dirname) 
    throws IOException
  {
    this.queueDir = dirname;
    File q = new File(dirname);
    if (!q.exists())
      q.mkdirs();
   
    if (!q.isDirectory())
      throw new IOException("name "+dirname
                            +" exits already, but not a directory.");
  }

  /** Enqueue a report id. */
  public boolean enqueue(String rid) {
    //lock on the directory
    // add a file named by id
    File f = new File(this.queueDir, rid);
    try {
      return f.createNewFile();
    } catch (IOException e) {
      // TODO Auto-generated catch block
      e.printStackTrace();
      return false;
    }
  }
  
  /** Enqueue a list of ids. */
  public boolean enqueue(LinkedList<String> ids) {
    //lock on the directory
    // add a file named by id
    for (String rid : ids) {
      File f = new File(this.queueDir, rid);
      try {
        if (!f.createNewFile())
          return false;
      } catch (IOException e) {
        // TODO Auto-generated catch block
        e.printStackTrace();
        return false;
      }
    }
    return true;
  }
  
  /** Checks if the queue is empty. */
  public boolean empty() {
    File f = new File(this.queueDir);
    String[] ids = f.list();
    if (ids == null)
      return true;
    else
      return ids.length == 0;
  }
  
  /** Remove ids from the queue. */
  public LinkedList<String> dequeue() {
    // lock on the directory
    LinkedList<String> rids = new LinkedList<String>();
    File d = new File(this.queueDir);
    String[] ids = d.list();
    if (ids == null)
      return rids;

    for (int i =0; i < Math.min(ids.length, MAX_DEQUEUED_IDS); i++) {
      File f = new File(this.queueDir, ids[i]);
      f.delete();
      rids.add(ids[i]);
    }
    
    return rids;
  }
}

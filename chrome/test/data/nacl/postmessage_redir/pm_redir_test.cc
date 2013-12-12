/*
 * Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

//
// Post-message based test for simple rpc based access to name services.
//

#include <string>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "native_client/src/include/nacl_base.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"


class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance);
  virtual ~MyInstance();
  virtual void HandleMessage(const pp::Var& message_data);

 private:
  DISALLOW_COPY_AND_ASSIGN(MyInstance);
};

// ---------------------------------------------------------------------------

MyInstance::MyInstance(PP_Instance instance)
    : pp::Instance(instance) {
}

MyInstance::~MyInstance() {
}

typedef std::map<std::string, std::string> KeyValueMap;

static void ParseMapEntry(KeyValueMap* map,
                          std::string const& entry) {
  std::string::size_type eq = entry.find('=');
  std::string key;
  std::string val = "";
  if (std::string::npos != eq) {
    key = entry.substr(0, eq);
    val = entry.substr(eq+1);
  } else {
    key = entry;
  }
  (*map)[key] = val;
}

//
// parse a name1=value1,name2=value2 string into a map, using NRVO. spaces
// are significant (!).
//
static KeyValueMap ParseMap(std::string const& str_map) {
  KeyValueMap nrvo;
  std::string::size_type s = 0;
  std::string::size_type comma;

  while (std::string::npos != (comma = str_map.find(',', s))) {
    std::string sub = str_map.substr(s, comma - s);
    s = comma + 1;
    ParseMapEntry(&nrvo, sub);
  }
  if (s != str_map.size()) {
    std::string sub = str_map.substr(s);
    ParseMapEntry(&nrvo, sub);
  }
  return nrvo;
}


static std::string quotes[] = {
  "In the year 1878 I took my degree of Doctor of Medicine...\n",
  "A merry little surge of electricity piped by automatic alarm...\n",
  "Squire Trelawney, Dr. Livesey, and the rest of these gentlemen...\n",
  ("It is a truth universally acknowledged,"
   " that a single man in possession...\n"),
};

void output_quote(int desc, size_t ix) {
  const char* out = quotes[ix].c_str();
  size_t len = strlen(out);
  (void) write(desc, out, len);  /* assumes no partial writes! */
}

//
// thread start function -- output asynchronously.
//
/* calling conv */ extern "C" {
static void* bg_thread(void* state) {
  KeyValueMap* kvm(reinterpret_cast<KeyValueMap*>(state));

  std::string out = (*kvm)["stream"];
  std::string sleep_str;
  int sleep_us = 0;

  if ((*kvm).find("delay_us") != (*kvm).end()) {
    sleep_str = (*kvm)["delay_us"];
    if (sleep_str.length() > 0) {
      sleep_us = strtoul(sleep_str.c_str(), 0, 0);
    }
  }
  // Try to check that output works when the event handler thread has
  // already returned back to JavaScript, as opposed to output
  // occuring while the plugin is still executing the event handler
  // and blocking the JavaScript main thread.
  if (sleep_us > 0) {
    usleep(sleep_us);
  }
  if (out == "stdout") {
    output_quote(1, 2);
  } else if (out == "stderr") {
    output_quote(2, 3);
  } else {
    fprintf(stderr, "bg_thread: unrecognized output stream %s\n",
            out.c_str());
  }
  return NULL;
}
}  // extern "C"

// HandleMessage gets invoked when postMessage is called on the DOM
// element associated with this plugin instance.  In this case, if we
// are given a string, we'll post a message back to JavaScript with a
// reply -- essentially treating this as a string-based RPC.
void MyInstance::HandleMessage(const pp::Var& message) {
  std::string msg = "None";
  if (message.is_string()) {
    msg = message.AsString();
    KeyValueMap test_arg(ParseMap(msg));
    if (test_arg["thread"] == "fg") {
      std::string out = test_arg["stream"];
      if (out == "stdout") {
        output_quote(1, 0);
      } else if (out == "stderr") {
        output_quote(2, 1);
      } else {
        fprintf(stderr, "HandleMessage: unrecognized output stream %s\n",
                out.c_str());
      }
    } else {
      /* spawn thread to do output */
      pthread_t tid;

      pthread_create(&tid,
                     reinterpret_cast<const pthread_attr_t *>(NULL),
                     bg_thread,
                     reinterpret_cast<void *>(new KeyValueMap(test_arg)));
      pthread_detach(tid);
    }
  } else {
    fprintf(stderr, "HandleMessage: message is not a string\n");
    fflush(NULL);
  }
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

  // Override CreateInstance to create your customized Instance object.
  virtual pp::Instance *CreateInstance(PP_Instance instance);

  DISALLOW_COPY_AND_ASSIGN(MyModule);
};

pp::Instance *MyModule::CreateInstance(PP_Instance pp_instance) {
  return new MyInstance(pp_instance);
}

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp

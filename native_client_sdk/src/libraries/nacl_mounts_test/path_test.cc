/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fcntl.h>
#include "nacl_mounts/kernel_proxy.h"
#include "nacl_mounts/path.h"

#include "gtest/gtest.h"

TEST(PathTest, SanityChecks) {
  // can we construct and delete?
  Path ph1(".");
  Path *ph2 = new Path(".");
  delete ph2;

  Path p1(".");
  EXPECT_FALSE(p1.IsAbsolute());
  EXPECT_EQ(".", p1.Join());
  Path p2("/");
  EXPECT_TRUE(p2.IsAbsolute());
  EXPECT_EQ("/", p2.Join());
}

TEST(PathTest, Assignment) {
  Path empty;
  Path dot(".");
  Path root("/");
  Path abs_str("/abs/from/string");
  Path rel_str("rel/from/string");
  Path self_str("./rel/from/string");

  EXPECT_EQ(0, empty.Size());
  EXPECT_FALSE(empty.IsAbsolute());
  EXPECT_EQ(std::string(""), empty.Join());

  EXPECT_EQ(1, dot.Size());
  EXPECT_FALSE(dot.IsAbsolute());
  EXPECT_EQ(std::string("."), dot.Join());

  EXPECT_EQ(1, root.Size());
  EXPECT_TRUE(root.IsAbsolute());
  EXPECT_EQ(std::string("/"), root.Join());

  EXPECT_EQ(4, abs_str.Size());
  EXPECT_TRUE(abs_str.IsAbsolute());
  EXPECT_EQ(std::string("/abs/from/string"), abs_str.Join());

  EXPECT_EQ(3, rel_str.Size());
  EXPECT_FALSE(rel_str.IsAbsolute());
  EXPECT_EQ(std::string("rel/from/string"), rel_str.Join());

  EXPECT_EQ(3, self_str.Size());
  EXPECT_FALSE(self_str.IsAbsolute());
  EXPECT_EQ(std::string("rel/from/string"), self_str.Join());

  empty = "";
  dot = ".";
  root = "/";
  abs_str = "/abs/from/assign";
  rel_str = "rel/from/assign";
  self_str = "./rel/from/assign";

  EXPECT_EQ(1, empty.Size());
  EXPECT_FALSE(empty.IsAbsolute());
  EXPECT_EQ(std::string("."), empty.Join());

  EXPECT_EQ(1, dot.Size());
  EXPECT_FALSE(dot.IsAbsolute());
  EXPECT_EQ(std::string("."), dot.Join());

  EXPECT_EQ(1, root.Size());
  EXPECT_TRUE(root.IsAbsolute());
  EXPECT_EQ(std::string("/"), root.Join());

  EXPECT_EQ(4, abs_str.Size());
  EXPECT_TRUE(abs_str.IsAbsolute());
  EXPECT_EQ(std::string("/abs/from/assign"), abs_str.Join());

  EXPECT_EQ(3, rel_str.Size());
  EXPECT_FALSE(rel_str.IsAbsolute());
  EXPECT_EQ(std::string("rel/from/assign"), rel_str.Join());

  EXPECT_EQ(3, self_str.Size());
  EXPECT_FALSE(self_str.IsAbsolute());
  EXPECT_EQ(std::string("rel/from/assign"), self_str.Join());

  Path cpy_str;
  cpy_str = empty;
  EXPECT_EQ(1, cpy_str.Size());
  EXPECT_FALSE(cpy_str.IsAbsolute());
  EXPECT_EQ(std::string("."), cpy_str.Join());

  cpy_str = dot;
  EXPECT_EQ(1, cpy_str.Size());
  EXPECT_FALSE(cpy_str.IsAbsolute());
  EXPECT_EQ(std::string("."), cpy_str.Join());

  cpy_str = root;
  EXPECT_EQ(1, cpy_str.Size());
  EXPECT_TRUE(cpy_str.IsAbsolute());
  EXPECT_EQ(std::string("/"), cpy_str.Join());

  cpy_str = abs_str;
  EXPECT_EQ(4, cpy_str.Size());
  EXPECT_TRUE(cpy_str.IsAbsolute());
  EXPECT_EQ(std::string("/abs/from/assign"), cpy_str.Join());

  cpy_str = rel_str;
  EXPECT_EQ(3, cpy_str.Size());
  EXPECT_FALSE(cpy_str.IsAbsolute());
  EXPECT_EQ(std::string("rel/from/assign"), cpy_str.Join());

  cpy_str = self_str;
  EXPECT_EQ(3, cpy_str.Size());
  EXPECT_FALSE(cpy_str.IsAbsolute());
  EXPECT_EQ(std::string("rel/from/assign"), cpy_str.Join());
}


TEST(PathTest, Collapse) {
  StringArray_t path_components;

  Path p1("/simple/splitter/test");
  path_components = p1.Split();
  EXPECT_EQ("/", path_components[0]);
  EXPECT_EQ("/", p1.Part(0));

  EXPECT_EQ("simple", path_components[1]);
  EXPECT_EQ("simple", p1.Part(1));

  EXPECT_EQ("splitter",path_components[2]);
  EXPECT_EQ("splitter",p1.Part(2));

  EXPECT_EQ("test", path_components[3]);
  EXPECT_EQ("test", p1.Part(3));

  Path p2("///simple//splitter///test/");
  path_components = p2.Split();
  EXPECT_EQ(4, static_cast<int>(path_components.size()));
  EXPECT_EQ(4, static_cast<int>(p2.Size()));
  EXPECT_EQ("/", path_components[0]);
  EXPECT_EQ("simple", path_components[1]);
  EXPECT_EQ("splitter", path_components[2]);
  EXPECT_EQ("test", path_components[3]);

  Path p3("sim/ple//spli/tter/te/st/");
  path_components = p3.Split();
  EXPECT_EQ(6, static_cast<int>(path_components.size()));
  EXPECT_FALSE(p3.IsAbsolute());
  EXPECT_EQ("sim", path_components[0]);
  EXPECT_EQ("ple", path_components[1]);
  EXPECT_EQ("spli", path_components[2]);
  EXPECT_EQ("tter", path_components[3]);
  EXPECT_EQ("te", path_components[4]);
  EXPECT_EQ("st", path_components[5]);

  Path p4("");
  path_components = p4.Split();
  EXPECT_EQ(1, static_cast<int>(path_components.size()));

  Path p5("/");
  path_components = p5.Split();
  EXPECT_EQ(1, static_cast<int>(path_components.size()));
}

TEST(PathTest, AppendAndJoin) {
  Path ph1("/usr/local/hi/there");

  EXPECT_EQ("/usr/local/hi/there", ph1.Join());
  ph1 = ph1.Append("..");
  EXPECT_EQ("/usr/local/hi", ph1.Join());
  ph1 = ph1.Append(".././././hi/there/../.././././");
  EXPECT_EQ("/usr/local", ph1.Join());
  ph1 = ph1.Append("../../../../../../../../././../");
  EXPECT_EQ("/", ph1.Join());
  ph1 = ph1.Append("usr/lib/../bin/.././etc/../local/../share");
  EXPECT_EQ("/usr/share", ph1.Join());

  Path ph2("./");
  EXPECT_EQ(".", ph2.Join());

  Path ph3("/");
  EXPECT_EQ("/", ph3.Join());
  ph3 = ph3.Append("");
  EXPECT_EQ("/", ph3.Join());
  ph3 = ph3.Append("USR/local/SHARE");
  EXPECT_EQ("/USR/local/SHARE", ph3.Join());
  ph3 = ph3.Append("///////////////////////////////");
  EXPECT_EQ("/USR/local/SHARE", ph3.Join());

  Path ph4("..");
  EXPECT_EQ("..", ph4.Join());
  ph4 = ph4.Append("/node1/node3/../../node1/./");
  EXPECT_EQ("../node1", ph4.Join());
  ph4 = ph4.Append("node4/../../node1/./node5");
  EXPECT_EQ("../node1/node5", ph4.Join());
}


TEST(PathTest, Invalid) {
  Path rooted("/usr/local");
  Path current("./usr/local");
  Path relative("usr/local");

  Path test;

  test = rooted;
  test.Append("../..");
  EXPECT_EQ("/", test.Join());

  test = rooted;
  test.Append("../../..");
  EXPECT_EQ("/", test.Join());

  test = rooted;
  test.Append("../../../foo");
  EXPECT_EQ("/foo", test.Join());

  test = current;
  test.Append("../..");
  EXPECT_EQ(".", test.Join());

  test = current;
  test.Append("../../..");
  EXPECT_EQ("..", test.Join());

  test = current;
  test.Append("../../../foo");
  EXPECT_EQ("../foo", test.Join());
 
  test = relative;
  test.Append("../..");
  EXPECT_EQ(".", test.Join());

  test = relative;
  test.Append("../../..");
  EXPECT_EQ("..", test.Join());

  test = relative;
  test.Append("../../../foo");
  EXPECT_EQ("../foo", test.Join());
}

TEST(PathTest, Range) {
  Path p("/an/absolute/path");

  // p's parts should be ["/", "an", "absolute", "path"].
  EXPECT_EQ("/an/absolute/path", p.Range(0, 4));
  EXPECT_EQ("an/absolute/path", p.Range(1, 4));
  EXPECT_EQ("absolute/path", p.Range(2, 4));
  EXPECT_EQ("path", p.Range(3, 4));

  EXPECT_EQ("/an/absolute", p.Range(0, 3));
  EXPECT_EQ("an/absolute", p.Range(1, 3));
  EXPECT_EQ("absolute", p.Range(2, 3));
}

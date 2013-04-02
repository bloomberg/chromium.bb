<?xml version="1.0" ?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="xml" encoding="UTF-8" indent="yes" />

<xsl:template match="/">
  <!-- insert docbook's DOCTYPE blurb -->
    <xsl:text disable-output-escaping = "yes"><![CDATA[
<!DOCTYPE appendix PUBLIC "-//OASIS//DTD DocBook XML V4.5//EN" "http://www.oasis-open.org/docbook/xml/4.5/docbookx.dtd" [
  <!ENTITY % BOOK_ENTITIES SYSTEM "Wayland.ent">
%BOOK_ENTITIES;
]>
]]></xsl:text>

  <section id="sect-Library-Client">
    <title>Client API</title>
    <para>Following is the Wayland library classes for clients
	  (<emphasis>libwayland-client</emphasis>). Note that most of the
	  procedures are related with IPC, which is the main responsibility of
	  the library.
    </para>

    <para>
    <variablelist>
    <xsl:apply-templates select="/doxygen/compounddef" />
    </variablelist>
    </para>

    <para>Methods for the respective classes.</para>

    <para>
    <variablelist>
    <xsl:apply-templates select="/doxygen/compounddef/sectiondef/memberdef" />
    </variablelist>
    </para>
  </section>
</xsl:template>

<xsl:template match="parameteritem">
    <varlistentry>
        <term>
          <xsl:value-of select="parameternamelist/parametername"/>
        </term>
      <listitem>
        <para>
          <xsl:value-of select="parameterdescription/para"/>
        </para>
      </listitem>
    </varlistentry>
</xsl:template>

<xsl:template match="parameterlist">
  <xsl:if test="parameteritem">
      <variablelist>
        <xsl:apply-templates select="parameteritem" />
      </variablelist>
  </xsl:if>
</xsl:template>

<xsl:template match="ref">
  <emphasis><xsl:value-of select="." /></emphasis>
</xsl:template>

<xsl:template match="simplesect[@kind='return']">
  <variablelist>
    <varlistentry>
      <term>Returns:</term>
      <listitem>
        <para>
          <xsl:value-of select="." />
        </para>
      </listitem>
    </varlistentry>
  </variablelist>
</xsl:template>

<xsl:template match="simplesect[@kind='see']">
  <itemizedlist>
    <listitem>
      <para>
        See also:
        <xsl:for-each select="para/ref">
          <emphasis><xsl:value-of select="."/><xsl:text> </xsl:text></emphasis>
        </xsl:for-each>
      </para>
    </listitem>
  </itemizedlist>
</xsl:template>

<xsl:template match="simplesect[@kind='since']">
  <itemizedlist>
    <listitem>
      <para>
        Since: <xsl:value-of select="para"/>
      </para>
    </listitem>
  </itemizedlist>
</xsl:template>

<xsl:template match="simplesect[@kind='note']">
  <emphasis>Note: <xsl:value-of select="."/></emphasis>
</xsl:template>

<xsl:template match="programlisting">
  <programlisting><xsl:value-of select="."/></programlisting>
</xsl:template>

<!-- this opens a para for each detaileddescription/para. I could not find a
     way to extract the right text for the description from the
     source otherwise. Downside: we can't use para for return value, "see
     also", etc.  because they're already inside a para. So they're lists.

     It also means we don't control the order of when something is added to
     the output, it matches the input file
     -->
<xsl:template match="detaileddescription/para">
  <para><xsl:apply-templates /></para>
</xsl:template>

<xsl:template match="detaileddescription">
  <xsl:apply-templates select="para" />
</xsl:template>

<!-- methods -->
<xsl:template match="memberdef" >
  <xsl:if test="@kind = 'function' and @static = 'no'">
    <varlistentry>
        <term>
          <xsl:value-of select="name"/>
        - <xsl:value-of select="briefdescription" />
        </term>
        <listitem>
          <para>
            <synopsis>
              <xsl:value-of select="definition"/><xsl:value-of select="argsstring"/>
            </synopsis>
          </para>
          <xsl:apply-templates select="detaileddescription" />
        </listitem>
    </varlistentry>
    </xsl:if>
</xsl:template>

<!-- classes -->
<xsl:template match="compounddef" >
    <xsl:if test="@kind = 'class' ">
    <varlistentry>
        <term>
            <xsl:value-of select="compoundname" />
            <xsl:if test="briefdescription">
                - <xsl:value-of select="briefdescription" />
            </xsl:if>
        </term>

        <listitem>
            <xsl:for-each select="detaileddescription/para">
            <para><xsl:value-of select="." /></para>
            </xsl:for-each>
        </listitem>
    </varlistentry>
    </xsl:if>
</xsl:template>
</xsl:stylesheet>

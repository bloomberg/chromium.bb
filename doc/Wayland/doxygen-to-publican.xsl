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
    <para>Following is the Wayland library interface for clients
	  (<emphasis>libwayland-client</emphasis>). Note that most of the
	  procedures are related with IPC, which is the main responsibility of
	  the library.
    </para>

    <para>
    <variablelist>
    <xsl:apply-templates select="/doxygen/compounddef/sectiondef/memberdef" />
    </variablelist>
    </para>
  </section>
</xsl:template>

<xsl:template match="memberdef" >
    <xsl:if test="@kind = 'function'">
  <varlistentry>

      <term>
      <xsl:value-of select="name" />
      </term>
    <listitem>
    <para>
      <xsl:value-of select="briefdescription" />
    </para>
    </listitem>
  </varlistentry>
  </xsl:if>
</xsl:template>

</xsl:stylesheet>
